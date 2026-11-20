#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "koitile_core.h"

static int g_pass = 0;
static int g_fail = 0;

static int g_colorEnabled = 1;
static char g_color[64] = "#2D89EF";
static char g_image[4096] = "";
static int g_imageScale = 100;
static int g_imageOffsetX = 0;
static int g_imageOffsetY = 0;
static int g_iconEnabled = 0;
static char g_iconPath[4096] = "";
static int g_iconSize = 64;
static int g_iconOffsetX = 0;
static int g_iconOffsetY = 0;

int kt_config_get(KtConfig* cfg, const char* id, const char* key, char* buf, int cch)
{
    const char* v = "";
    (void)cfg;
    (void)id;
    if (!key || !buf || cch <= 0) return KT_ERR;
    if (strcmp(key, "color") == 0) v = g_color;
    else if (strcmp(key, "image") == 0) v = g_image;
    else if (strcmp(key, "iconPath") == 0) v = g_iconPath;
    else if (strcmp(key, "name") == 0) v = "tile";
    if ((int)strlen(v) >= cch) return KT_ERR;
    strcpy(buf, v);
    return KT_OK;
}

int kt_config_get_int(KtConfig* cfg, const char* id, const char* key, int fallback)
{
    (void)cfg;
    (void)id;
    if (!key) return fallback;
    if (strcmp(key, "colorEnabled") == 0) return g_colorEnabled;
    if (strcmp(key, "imageScale") == 0) return g_imageScale;
    if (strcmp(key, "imageOffsetX") == 0) return g_imageOffsetX;
    if (strcmp(key, "imageOffsetY") == 0) return g_imageOffsetY;
    if (strcmp(key, "iconEnabled") == 0) return g_iconEnabled;
    if (strcmp(key, "iconSize") == 0) return g_iconSize;
    if (strcmp(key, "iconOffsetX") == 0) return g_iconOffsetX;
    if (strcmp(key, "iconOffsetY") == 0) return g_iconOffsetY;
    return fallback;
}

int kt_size_pixels(int size, int* width, int* height)
{
    if (!width || !height) return KT_ERR;
    if (size == 0) {
        *width = 48;
        *height = 48;
        return KT_OK;
    }
    if (size == 1) {
        *width = 100;
        *height = 100;
        return KT_OK;
    }
    if (size == 2) {
        *width = 204;
        *height = 100;
        return KT_OK;
    }
    if (size == 3) {
        *width = 204;
        *height = 204;
        return KT_OK;
    }
    return KT_ERR;
}

static void check(const char* what, int cond)
{
    if (cond) {
        g_pass++;
        printf("  PASS  %s\n", what);
    } else {
        g_fail++;
        printf("  FAIL  %s\n", what);
    }
}

static void check_dims(const char* label, const char* path, int ew, int eh)
{
    int w = -1;
    int h = -1;
    int rc = kt_image_info(path, &w, &h);
    char msg[1024];
    snprintf(msg, sizeof(msg), "%s exists+size %dx%d", label, ew, eh);
    check(msg, rc == KT_OK && w == ew && h == eh);
    if (rc != KT_OK || w != ew || h != eh) {
        printf("        actual rc=%d size=%dx%d path=%s\n", rc, w, h, path);
    }
}

static wchar_t* to_wide(const char* s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w;
    if (n <= 0) return NULL;
    w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static int ensure_dir_a(const char* dir)
{
    wchar_t* wd = to_wide(dir);
    DWORD attr;
    if (!wd) return 0;
    CreateDirectoryW(wd, NULL);
    attr = GetFileAttributesW(wd);
    free(wd);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

static int remove_tree_a(const char* dir)
{
    char pattern[4096];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char child[4096];
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            snprintf(child, sizeof(child), "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                remove_tree_a(child);
            } else {
                wchar_t* wc = to_wide(child);
                if (wc) {
                    DeleteFileW(wc);
                    free(wc);
                }
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    {
        wchar_t* wd = to_wide(dir);
        if (wd) {
            RemoveDirectoryW(wd);
            free(wd);
        }
    }
    return 1;
}

static int scan_for_suffix(const char* dir, const char* suffix, char* hit, int cch)
{
    char pattern[4096];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    size_t sl = strlen(suffix);
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char child[4096];
            size_t nl;
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            snprintf(child, sizeof(child), "%s\\%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (scan_for_suffix(child, suffix, hit, cch)) {
                    FindClose(h);
                    return 1;
                }
                continue;
            }
            nl = strlen(fd.cFileName);
            if (nl >= sl && strcmp(fd.cFileName + nl - sl, suffix) == 0) {
                snprintf(hit, (size_t)cch, "%s", child);
                FindClose(h);
                return 1;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return 0;
}

static int file_exists_a(const char* path)
{
    wchar_t* w = to_wide(path);
    DWORD attr;
    if (!w) return 0;
    attr = GetFileAttributesW(w);
    free(w);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
}

static void put_u16(unsigned char* p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void put_u32(unsigned char* p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static int make_ico_solid(const char* path, int n, const int* sizes, const unsigned char* bs, const unsigned char* gs, const unsigned char* rs)
{
    unsigned char* buf;
    int* offs;
    int total;
    int off;
    int i;
    FILE* fp;
    if (n <= 0 || !path) return 0;
    offs = (int*)malloc(sizeof(int) * (size_t)n);
    if (!offs) return 0;
    total = 6 + 16 * n;
    off = total;
    for (i = 0; i < n; i++) {
        int w = sizes[i];
        int andRow = ((w + 31) / 32) * 4;
        offs[i] = off;
        off += 40 + w * w * 4 + andRow * w;
    }
    total = off;
    buf = (unsigned char*)calloc(1, (size_t)total);
    if (!buf) {
        free(offs);
        return 0;
    }
    put_u16(buf + 0, 0);
    put_u16(buf + 2, 1);
    put_u16(buf + 4, (unsigned int)n);
    for (i = 0; i < n; i++) {
        unsigned char* e = buf + 6 + 16 * i;
        unsigned char* d = buf + offs[i];
        int w = sizes[i];
        int xorBytes = w * w * 4;
        int andRow = ((w + 31) / 32) * 4;
        int y;
        e[0] = (unsigned char)(w >= 256 ? 0 : w);
        e[1] = e[0];
        e[2] = 0;
        e[3] = 0;
        put_u16(e + 4, 1);
        put_u16(e + 6, 32);
        put_u32(e + 8, (unsigned int)(40 + xorBytes + andRow * w));
        put_u32(e + 12, (unsigned int)offs[i]);
        put_u32(d + 0, 40);
        put_u32(d + 4, (unsigned int)w);
        put_u32(d + 8, (unsigned int)(w * 2));
        put_u16(d + 12, 1);
        put_u16(d + 14, 32);
        put_u32(d + 16, 0);
        put_u32(d + 20, (unsigned int)xorBytes);
        put_u32(d + 24, 0);
        put_u32(d + 28, 0);
        put_u32(d + 32, 0);
        put_u32(d + 36, 0);
        for (y = 0; y < w * w; y++) {
            d[40 + y * 4 + 0] = bs[i];
            d[40 + y * 4 + 1] = gs[i];
            d[40 + y * 4 + 2] = rs[i];
            d[40 + y * 4 + 3] = 255;
        }
    }
    fp = fopen(path, "wb");
    if (!fp) {
        free(buf);
        free(offs);
        return 0;
    }
    fwrite(buf, 1, (size_t)total, fp);
    fclose(fp);
    free(buf);
    free(offs);
    return 1;
}

static int make_png_ex(const char* path, const GUID* container, int w, int h, int r, int g, int b, int solid);

static int make_png(const char* path, int w, int h, int r, int g, int b, int solid)
{
    return make_png_ex(path, &GUID_ContainerFormatPng, w, h, r, g, b, solid);
}

static int make_png_ex(const char* path, const GUID* container, int w, int h, int r, int g, int b, int solid)
{
    int owned = 0;
    HRESULT hr;
    IWICImagingFactory* f = NULL;
    IWICBitmap* bmp = NULL;
    IWICStream* st = NULL;
    IWICBitmapEncoder* enc = NULL;
    IWICBitmapFrameEncode* fr = NULL;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    unsigned char* buf;
    wchar_t* wp;
    int i;
    int ok = 0;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == S_OK || hr == S_FALSE) owned = 1;
    else if (hr != RPC_E_CHANGED_MODE) return 0;
    wp = to_wide(path);
    if (!wp) {
        if (owned) CoUninitialize();
        return 0;
    }
    buf = (unsigned char*)malloc((size_t)w * h * 4);
    if (!buf) {
        free(wp);
        if (owned) CoUninitialize();
        return 0;
    }
    for (i = 0; i < w * h; i++) {
        int x = i % w;
        int y = i / w;
        buf[i * 4 + 0] = solid ? (unsigned char)b : (unsigned char)((b + x) & 0xFF);
        buf[i * 4 + 1] = solid ? (unsigned char)g : (unsigned char)((g + y) & 0xFF);
        buf[i * 4 + 2] = (unsigned char)r;
        buf[i * 4 + 3] = 255;
    }
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&f);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateBitmapFromMemory(f, (UINT)w, (UINT)h, &GUID_WICPixelFormat32bppBGRA, (UINT)(w * 4), (UINT)(w * 4 * h), buf, &bmp);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateStream(f, &st);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromFilename(st, wp, GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(f, container, NULL, &enc);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(enc, (IStream*)st, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(enc, &fr, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(fr, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(fr, (UINT)w, (UINT)h);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(fr, &fmt);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WriteSource(fr, (IWICBitmapSource*)bmp, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(fr);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(enc);
    if (SUCCEEDED(hr)) ok = 1;
    if (fr) IWICBitmapFrameEncode_Release(fr);
    if (enc) IWICBitmapEncoder_Release(enc);
    if (st) IWICStream_Release(st);
    if (bmp) IWICBitmap_Release(bmp);
    if (f) IWICImagingFactory_Release(f);
    free(buf);
    free(wp);
    if (owned) CoUninitialize();
    return ok;
}

static int read_pixel(const char* path, int x, int y, unsigned char* out)
{
    int owned = 0;
    HRESULT hr;
    IWICImagingFactory* f = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* fr = NULL;
    IWICFormatConverter* conv = NULL;
    wchar_t* wp;
    WICRect rc;
    int ok = 0;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == S_OK || hr == S_FALSE) owned = 1;
    else if (hr != RPC_E_CHANGED_MODE) return 0;
    wp = to_wide(path);
    if (!wp) {
        if (owned) CoUninitialize();
        return 0;
    }
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&f);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateDecoderFromFilename(f, wp, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(dec, 0, &fr);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateFormatConverter(f, &conv);
    if (SUCCEEDED(hr)) hr = IWICFormatConverter_Initialize(conv, (IWICBitmapSource*)fr, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    rc.X = x;
    rc.Y = y;
    rc.Width = 1;
    rc.Height = 1;
    if (SUCCEEDED(hr)) hr = IWICBitmapSource_CopyPixels((IWICBitmapSource*)conv, &rc, 4, 4, out);
    if (SUCCEEDED(hr)) ok = 1;
    if (conv) IWICFormatConverter_Release(conv);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (dec) IWICBitmapDecoder_Release(dec);
    if (f) IWICImagingFactory_Release(f);
    free(wp);
    if (owned) CoUninitialize();
    return ok;
}

static void check_pixel(const char* label, const char* path, int x, int y, int eb, int eg, int er, int ea)
{
    unsigned char px[4];
    char msg[1024];
    int ok;
    px[0] = 0;
    px[1] = 0;
    px[2] = 0;
    px[3] = 0;
    ok = read_pixel(path, x, y, px);
    snprintf(msg, sizeof(msg), "%s pixel(%d,%d) BGRA=(%d,%d,%d,%d)", label, x, y, eb, eg, er, ea);
    check(msg, ok && px[0] == eb && px[1] == eg && px[2] == er && px[3] == ea);
    if (!ok || px[0] != eb || px[1] != eg || px[2] != er || px[3] != ea) {
        printf("        actual ok=%d BGRA=(%d,%d,%d,%d)\n", ok, px[0], px[1], px[2], px[3]);
    }
}

static void check_pixels_differ(const char* label, const char* pa_path, const char* pb_path, int x, int y)
{
    unsigned char pa[4];
    unsigned char pb[4];
    int oka;
    int okb;
    pa[0] = 0; pa[1] = 0; pa[2] = 0; pa[3] = 0;
    pb[0] = 0; pb[1] = 0; pb[2] = 0; pb[3] = 0;
    oka = read_pixel(pa_path, x, y, pa);
    okb = read_pixel(pb_path, x, y, pb);
    check(label, oka && okb && memcmp(pa, pb, 4) != 0);
    if (!oka || !okb || memcmp(pa, pb, 4) == 0) {
        printf("        a=(%d,%d,%d,%d) b=(%d,%d,%d,%d)\n", pa[0], pa[1], pa[2], pa[3], pb[0], pb[1], pb[2], pb[3]);
    }
}

int main(void)
{
    static const char* SHELL32 = "C:\\Windows\\System32\\shell32.dll";
    static const char* ICO_SAMPLE = "C:\\Windows\\System32\\OneDrive.ico";
    static int cfg_token = 0;
    KtConfig* cfg = (KtConfig*)&cfg_token;
    char path[4096];
    char hit[4096];
    int w = -1;
    int h = -1;
    int count = -1;
    int i;
    int rc;

    printf("koiTile image module tests\n");

    ensure_dir_a("bin");
    remove_tree_a("bin\\imgtest");
    if (!ensure_dir_a("bin\\imgtest")) {
        printf("cannot create bin\\imgtest\n");
        return 2;
    }

    printf("[1] generate 400x300 test png\n");
    check("make_png bin\\imgtest\\src400x300.png", make_png("bin\\imgtest\\src400x300.png", 400, 300, 45, 137, 239, 0));
    check("make_png bin\\imgtest\\icon100.png", make_png("bin\\imgtest\\icon100.png", 100, 100, 220, 60, 60, 0));
    check("make_png bin\\imgtest\\solid40.png", make_png("bin\\imgtest\\solid40.png", 40, 40, 30, 20, 10, 1));

    printf("[2] kt_image_info\n");
    rc = kt_image_info("bin\\imgtest\\src400x300.png", &w, &h);
    check("info(src400x300) rc=KT_OK", rc == KT_OK);
    check("info(src400x300) = 400x300", w == 400 && h == 300);
    printf("        actual %dx%d\n", w, h);
    rc = kt_image_info("bin\\imgtest\\nope.png", &w, &h);
    check("info(missing) = KT_ERR", rc == KT_ERR);

    printf("[2b] kt_image_info other containers\n");
    check("make jpeg 64x48", make_png_ex("bin\\imgtest\\sample.jpg", &GUID_ContainerFormatJpeg, 64, 48, 200, 100, 50, 1));
    check("make bmp 64x48", make_png_ex("bin\\imgtest\\sample.bmp", &GUID_ContainerFormatBmp, 64, 48, 200, 100, 50, 1));
    check("make gif 64x48", make_png_ex("bin\\imgtest\\sample.gif", &GUID_ContainerFormatGif, 64, 48, 200, 100, 50, 1));
    check("make tiff 64x48", make_png_ex("bin\\imgtest\\sample.tiff", &GUID_ContainerFormatTiff, 64, 48, 200, 100, 50, 1));
    rc = kt_image_info("bin\\imgtest\\sample.jpg", &w, &h);
    check("info(jpg) = 64x48", rc == KT_OK && w == 64 && h == 48);
    rc = kt_image_info("bin\\imgtest\\sample.bmp", &w, &h);
    check("info(bmp) = 64x48", rc == KT_OK && w == 64 && h == 48);
    rc = kt_image_info("bin\\imgtest\\sample.gif", &w, &h);
    check("info(gif) = 64x48", rc == KT_OK && w == 64 && h == 48);
    rc = kt_image_info("bin\\imgtest\\sample.tiff", &w, &h);
    check("info(tiff) = 64x48", rc == KT_OK && w == 64 && h == 48);
    rc = kt_image_resize("bin\\imgtest\\sample.jpg", 20, 10, "bin\\imgtest\\from_jpg.png");
    check("resize from jpg rc=KT_OK", rc == KT_OK);
    check_dims("from_jpg", "bin\\imgtest\\from_jpg.png", 20, 10);
    rc = kt_image_extract_icon("bin\\imgtest\\sample.bmp", 0, 32, "bin\\imgtest\\from_bmp_icon.png");
    check("extract_icon from bmp rc=KT_OK", rc == KT_OK);
    check_dims("from_bmp_icon", "bin\\imgtest\\from_bmp_icon.png", 32, 32);

    printf("[3] kt_image_resize\n");
    rc = kt_image_resize("bin\\imgtest\\src400x300.png", 128, 96, "bin\\imgtest\\resize128x96.png");
    check("resize 128x96 rc=KT_OK", rc == KT_OK);
    check_dims("resize128x96", "bin\\imgtest\\resize128x96.png", 128, 96);
    rc = kt_image_resize("bin\\imgtest\\src400x300.png", 37, 53, "bin\\imgtest\\nested\\deep\\resize37x53.png");
    check("resize 37x53 into new dirs rc=KT_OK", rc == KT_OK);
    check_dims("resize37x53", "bin\\imgtest\\nested\\deep\\resize37x53.png", 37, 53);
    rc = kt_image_resize("bin\\imgtest\\nope.png", 32, 32, "bin\\imgtest\\should_not_exist.png");
    check("resize(missing src) = KT_ERR", rc == KT_ERR);
    check("resize(missing src) left no file", !file_exists_a("bin\\imgtest\\should_not_exist.png"));
    rc = kt_image_resize("bin\\imgtest\\src400x300.png", 32, 32, "bin\\imgtest\\src400x300.png\\bad.png");
    check("resize(dir is a file) = KT_ERR", rc == KT_ERR);
    rc = kt_image_resize("bin\\imgtest\\solid40.png", 64, 64, "bin\\imgtest\\solid64.png");
    check("resize solid 40x40 -> 64x64 rc=KT_OK", rc == KT_OK);
    check_pixel("solid64", "bin\\imgtest\\solid64.png", 0, 0, 10, 20, 30, 255);
    check_pixel("solid64", "bin\\imgtest\\solid64.png", 63, 63, 10, 20, 30, 255);

    printf("[4] kt_image_cut_grid 2x3\n");
    count = -1;
    rc = kt_image_cut_grid("bin\\imgtest\\src400x300.png", 2, 3, 32, 24, "grid", "bin\\imgtest\\grid", &count);
    check("cut_grid 2x3 rc=KT_OK", rc == KT_OK);
    check("cut_grid 2x3 count=6", count == 6);
    printf("        actual count=%d\n", count);
    for (i = 1; i <= 6; i++) {
        snprintf(path, sizeof(path), "bin\\imgtest\\grid\\grid_32x24_%d.png", i);
        check_dims(path, path, 32, 24);
    }
    check("cut_grid extra 7th file absent", !file_exists_a("bin\\imgtest\\grid\\grid_32x24_7.png"));

    printf("[5] kt_image_cut_grid 1x1\n");
    count = -1;
    rc = kt_image_cut_grid("bin\\imgtest\\src400x300.png", 1, 1, 64, 64, "single", "bin\\imgtest\\single", &count);
    check("cut_grid 1x1 rc=KT_OK", rc == KT_OK);
    check("cut_grid 1x1 count=1", count == 1);
    check_dims("single_64x64", "bin\\imgtest\\single\\single_64x64.png", 64, 64);
    check("cut_grid 1x1 no index suffix", !file_exists_a("bin\\imgtest\\single\\single_64x64_1.png"));

    printf("[6] kt_image_cut_grid empty target name\n");
    count = -1;
    rc = kt_image_cut_grid("bin\\imgtest\\src400x300.png", 1, 2, 16, 16, "", "bin\\imgtest\\defname", &count);
    check("cut_grid empty target rc=KT_OK", rc == KT_OK);
    check("cut_grid empty target count=2", count == 2);
    check_dims("tile_16x16_1", "bin\\imgtest\\defname\\tile_16x16_1.png", 16, 16);
    check_dims("tile_16x16_2", "bin\\imgtest\\defname\\tile_16x16_2.png", 16, 16);

    printf("[7] kt_image_composite\n");
    rc = kt_image_composite("bin\\imgtest\\src400x300.png", "bin\\imgtest\\icon100.png", 32, 5, -3, 200, 150, "bin\\imgtest\\composite200x150.png");
    check("composite rc=KT_OK", rc == KT_OK);
    check_dims("composite200x150", "bin\\imgtest\\composite200x150.png", 200, 150);
    rc = kt_image_composite("bin\\imgtest\\nope.png", NULL, 0, 0, 0, 64, 64, "bin\\imgtest\\composite_bad.png");
    check("composite(missing bg) = KT_ERR", rc == KT_ERR);
    check("composite(missing bg) left no file", !file_exists_a("bin\\imgtest\\composite_bad.png"));

    printf("[8] kt_image_extract_icon\n");
    rc = kt_image_extract_icon(SHELL32, 0, 48, "bin\\imgtest\\shell32_0_48.png");
    check("extract_icon shell32.dll#0 rc=KT_OK", rc == KT_OK);
    check_dims("shell32_0_48", "bin\\imgtest\\shell32_0_48.png", 48, 48);
    rc = kt_image_extract_icon(SHELL32, 1, 96, "bin\\imgtest\\shell32_1_96.png");
    check("extract_icon shell32.dll#1 96 rc=KT_OK", rc == KT_OK);
    check_dims("shell32_1_96", "bin\\imgtest\\shell32_1_96.png", 96, 96);
    rc = kt_image_extract_icon("bin\\imgtest\\icon100.png", 0, 128, "bin\\imgtest\\png_icon_128.png");
    check("extract_icon png rc=KT_OK", rc == KT_OK);
    check_dims("png_icon_128", "bin\\imgtest\\png_icon_128.png", 128, 128);
    rc = kt_image_extract_icon("C:\\Windows\\System32\\does-not-exist.dll", 0, 32, "bin\\imgtest\\bad_icon.png");
    check("extract_icon(missing) = KT_ERR", rc == KT_ERR);
    check("extract_icon(missing) left no file", !file_exists_a("bin\\imgtest\\bad_icon.png"));
    if (file_exists_a(ICO_SAMPLE)) {
        rc = kt_image_extract_icon(ICO_SAMPLE, 0, 64, "bin\\imgtest\\ico_wic_64.png");
        check("extract_icon .ico via WIC rc=KT_OK", rc == KT_OK);
        check_dims("ico_wic_64", "bin\\imgtest\\ico_wic_64.png", 64, 64);
    } else {
        printf("  SKIP  no .ico sample at %s\n", ICO_SAMPLE);
    }

    printf("[8b] extract_icon multi-frame .ico picks the largest frame\n");
    {
        int sizes[2];
        unsigned char bs[2];
        unsigned char gs[2];
        unsigned char rs[2];
        sizes[0] = 16;
        sizes[1] = 64;
        bs[0] = 255;
        bs[1] = 0;
        gs[0] = 0;
        gs[1] = 0;
        rs[0] = 0;
        rs[1] = 255;
        check("make_ico_solid 16x16 blue + 64x64 red", make_ico_solid("bin\\imgtest\\multi.ico", 2, sizes, bs, gs, rs));
        rc = kt_image_extract_icon("bin\\imgtest\\multi.ico", 0, 64, "bin\\imgtest\\multi_ico_64.png");
        check("extract_icon multi-frame ico rc=KT_OK", rc == KT_OK);
        check_dims("multi_ico_64", "bin\\imgtest\\multi_ico_64.png", 64, 64);
        check_pixel("multi_ico_64 is red 64x64 frame", "bin\\imgtest\\multi_ico_64.png", 0, 0, 0, 0, 255, 255);
        check_pixel("multi_ico_64 is red 64x64 frame", "bin\\imgtest\\multi_ico_64.png", 63, 63, 0, 0, 255, 255);
        rc = kt_image_extract_icon("bin\\imgtest\\multi.ico", 0, 16, "bin\\imgtest\\multi_ico_16.png");
        check("extract_icon multi-frame ico 16 rc=KT_OK", rc == KT_OK);
        check_dims("multi_ico_16", "bin\\imgtest\\multi_ico_16.png", 16, 16);
        check_pixel("multi_ico_16 still red 64x64 frame", "bin\\imgtest\\multi_ico_16.png", 0, 0, 0, 0, 255, 255);
    }

    printf("[9] kt_image_render_tile four sizes\n");
    g_colorEnabled = 1;
    snprintf(g_color, sizeof(g_color), "%s", "#2D89EF");
    snprintf(g_image, sizeof(g_image), "%s", "bin\\imgtest\\src400x300.png");
    g_imageScale = 150;
    g_imageOffsetX = 10;
    g_imageOffsetY = -5;
    g_iconEnabled = 1;
    snprintf(g_iconPath, sizeof(g_iconPath), "%s", SHELL32);
    g_iconSize = 64;
    g_iconOffsetX = 4;
    g_iconOffsetY = -4;
    {
        int sw[4];
        int sh[4];
        sw[0] = 48; sh[0] = 48;
        sw[1] = 100; sh[1] = 100;
        sw[2] = 204; sh[2] = 100;
        sw[3] = 204; sh[3] = 204;
        for (i = 0; i < 4; i++) {
            snprintf(path, sizeof(path), "bin\\imgtest\\tile_%d.png", i);
            rc = kt_image_render_tile(cfg, "tile-1", i, path);
            check("render_tile rc=KT_OK", rc == KT_OK);
            check_dims("render_tile", path, sw[i], sh[i]);
        }
    }

    printf("[9b] icon overlay changes pixels\n");
    snprintf(g_iconPath, sizeof(g_iconPath), "%s", "bin\\imgtest\\icon100.png");
    g_iconSize = 64;
    g_iconOffsetX = 4;
    g_iconOffsetY = -4;
    g_iconEnabled = 0;
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_LARGE, "bin\\imgtest\\tile_noicon.png");
    check("render_tile without icon rc=KT_OK", rc == KT_OK);
    g_iconEnabled = 1;
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_LARGE, "bin\\imgtest\\tile_withicon.png");
    check("render_tile with icon rc=KT_OK", rc == KT_OK);
    check_pixels_differ("icon overlay changed center pixel", "bin\\imgtest\\tile_withicon.png", "bin\\imgtest\\tile_noicon.png", 102, 102);

    printf("[9c] render_tile icon from .ico via WIC largest frame\n");
    if (file_exists_a(ICO_SAMPLE)) {
        snprintf(g_iconPath, sizeof(g_iconPath), "%s", ICO_SAMPLE);
        g_iconEnabled = 1;
        g_iconSize = 80;
        rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_LARGE, "bin\\imgtest\\tile_ico_icon.png");
        check("render_tile .ico icon rc=KT_OK", rc == KT_OK);
        check_dims("tile_ico_icon", "bin\\imgtest\\tile_ico_icon.png", 204, 204);
    } else {
        printf("  SKIP  no .ico sample at %s\n", ICO_SAMPLE);
    }

    printf("[9d] render_tile multi-frame .ico picks the largest frame\n");
    {
        int sizes[2];
        unsigned char bs[2];
        unsigned char gs[2];
        unsigned char rs[2];
        sizes[0] = 16;
        sizes[1] = 64;
        bs[0] = 255;
        bs[1] = 0;
        gs[0] = 0;
        gs[1] = 0;
        rs[0] = 0;
        rs[1] = 255;
        check("make_ico_solid 16x16 blue + 64x64 red", make_ico_solid("bin\\imgtest\\multi.ico", 2, sizes, bs, gs, rs));
        g_colorEnabled = 1;
        snprintf(g_color, sizeof(g_color), "%s", "#000000");
        g_image[0] = 0;
        g_iconEnabled = 1;
        snprintf(g_iconPath, sizeof(g_iconPath), "%s", "bin\\imgtest\\multi.ico");
        g_iconSize = 64;
        g_iconOffsetX = 0;
        g_iconOffsetY = 0;
        rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_LARGE, "bin\\imgtest\\tile_ico_multi.png");
        check("render_tile multi-frame ico rc=KT_OK", rc == KT_OK);
        check_dims("tile_ico_multi", "bin\\imgtest\\tile_ico_multi.png", 204, 204);
        check_pixel("tile_ico_multi center is red 64x64 frame", "bin\\imgtest\\tile_ico_multi.png", 102, 102, 0, 0, 255, 255);
        check_pixel("tile_ico_multi corner is black fill", "bin\\imgtest\\tile_ico_multi.png", 0, 0, 0, 0, 0, 255);
    }

    printf("[10] kt_image_render_tile color only / image only\n");
    g_colorEnabled = 1;
    snprintf(g_color, sizeof(g_color), "%s", "#112233");
    g_image[0] = 0;
    g_iconEnabled = 0;
    g_iconPath[0] = 0;
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_WIDE, "bin\\imgtest\\tile_color_only.png");
    check("render_tile color only rc=KT_OK", rc == KT_OK);
    check_dims("tile_color_only", "bin\\imgtest\\tile_color_only.png", 204, 100);
    check_pixel("tile_color_only #112233", "bin\\imgtest\\tile_color_only.png", 0, 0, 51, 34, 17, 255);
    check_pixel("tile_color_only #112233", "bin\\imgtest\\tile_color_only.png", 203, 99, 51, 34, 17, 255);
    g_colorEnabled = 0;
    snprintf(g_image, sizeof(g_image), "%s", "bin\\imgtest\\icon100.png");
    g_imageScale = 5;
    g_imageOffsetX = 0;
    g_imageOffsetY = 0;
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_SMALL, "bin\\imgtest\\tile_image_clamped.png");
    check("render_tile scale clamp rc=KT_OK", rc == KT_OK);
    check_dims("tile_image_clamped", "bin\\imgtest\\tile_image_clamped.png", 48, 48);
    check_pixel("tile_image_clamped no default fill", "bin\\imgtest\\tile_image_clamped.png", 0, 0, 0, 0, 0, 0);
    g_colorEnabled = 0;
    g_image[0] = 0;
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_MEDIUM, "bin\\imgtest\\tile_empty.png");
    check("render_tile empty cfg rc=KT_OK", rc == KT_OK);
    check_dims("tile_empty", "bin\\imgtest\\tile_empty.png", 100, 100);
    check_pixel("tile_empty default #2D89EF", "bin\\imgtest\\tile_empty.png", 0, 0, 239, 137, 45, 255);
    check_pixel("tile_empty default #2D89EF", "bin\\imgtest\\tile_empty.png", 99, 99, 239, 137, 45, 255);
    g_colorEnabled = 0;
    snprintf(g_image, sizeof(g_image), "%s", "bin\\imgtest\\no-such-image.png");
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_MEDIUM, "bin\\imgtest\\tile_badimage.png");
    check("render_tile undecodable image rc=KT_OK", rc == KT_OK);
    check_dims("tile_badimage", "bin\\imgtest\\tile_badimage.png", 100, 100);
    check_pixel("tile_badimage default #2D89EF", "bin\\imgtest\\tile_badimage.png", 5, 5, 239, 137, 45, 255);
    g_image[0] = 0;
    g_colorEnabled = 1;
    snprintf(g_color, sizeof(g_color), "%s", "not-a-color");
    rc = kt_image_render_tile(cfg, "tile-1", KT_SIZE_SMALL, "bin\\imgtest\\tile_badcolor.png");
    check("render_tile invalid color rc=KT_OK", rc == KT_OK);
    check_dims("tile_badcolor", "bin\\imgtest\\tile_badcolor.png", 48, 48);
    check_pixel("tile_badcolor stays transparent", "bin\\imgtest\\tile_badcolor.png", 0, 0, 0, 0, 0, 0);

    printf("[11] no temp file left behind\n");
    hit[0] = 0;
    check("no .koitile-tmp leftovers", !scan_for_suffix("bin\\imgtest", ".koitile-tmp", hit, sizeof(hit)));
    if (hit[0]) printf("        leftover=%s\n", hit);

    printf("\npassed=%d failed=%d\n", g_pass, g_fail);
    if (g_fail == 0) printf("ALL TESTS PASSED\n");
    else printf("TESTS FAILED\n");
    return g_fail == 0 ? 0 : 1;
}
