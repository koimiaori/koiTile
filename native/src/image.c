#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <wincodec.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#include "koitile_core.h"

#define KT_TMP_SUFFIX_A ".koitile-tmp"
#define KT_PATH_MAX 4096
#define KT_DEFAULT_TILE_COLOR "#2D89EF"

static wchar_t* kt_utf8_to_wide(const char* s)
{
    int n;
    wchar_t* w;
    if (!s) return NULL;
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

static char* kt_wide_to_utf8(const wchar_t* s)
{
    int n;
    char* b;
    if (!s) return NULL;
    n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    b = (char*)malloc((size_t)n);
    if (!b) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, s, -1, b, n, NULL, NULL) <= 0) {
        free(b);
        return NULL;
    }
    return b;
}

static int kt_abs_path_utf8(const char* path, char* out, int cch)
{
    wchar_t* wp;
    wchar_t* wfull;
    char* u8;
    DWORD n;
    int rc = KT_ERR;
    if (!path || !out || cch <= 1) return KT_ERR;
    wp = kt_utf8_to_wide(path);
    if (!wp) return KT_ERR;
    wfull = (wchar_t*)malloc(4096 * sizeof(wchar_t));
    if (!wfull) {
        free(wp);
        return KT_ERR;
    }
    n = GetFullPathNameW(wp, 4096, wfull, NULL);
    if (n > 0 && n < 4096) {
        u8 = kt_wide_to_utf8(wfull);
        if (u8) {
            if ((int)strlen(u8) < cch) {
                strcpy(out, u8);
                rc = KT_OK;
            }
            free(u8);
        }
    }
    free(wfull);
    free(wp);
    return rc;
}

static int kt_mkdir_recursive_wide(const wchar_t* wdir)
{
    size_t len;
    wchar_t* buf;
    size_t i;
    DWORD attr;
    if (!wdir || !wdir[0]) return KT_OK;
    len = wcslen(wdir);
    buf = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
    if (!buf) return KT_ERR;
    wcscpy(buf, wdir);
    for (i = 0; i < len; i++) {
        if ((buf[i] == L'\\' || buf[i] == L'/') && i > 0) {
            wchar_t sep;
            if (i == 2 && buf[1] == L':') continue;
            sep = buf[i];
            buf[i] = 0;
            CreateDirectoryW(buf, NULL);
            buf[i] = sep;
        }
    }
    CreateDirectoryW(buf, NULL);
    attr = GetFileAttributesW(buf);
    free(buf);
    if (attr == INVALID_FILE_ATTRIBUTES) return KT_ERR;
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) return KT_ERR;
    return KT_OK;
}

static int kt_ensure_dir(const char* dir)
{
    wchar_t* wd;
    int rc;
    if (!dir || !dir[0]) return KT_OK;
    wd = kt_utf8_to_wide(dir);
    if (!wd) return KT_ERR;
    rc = kt_mkdir_recursive_wide(wd);
    free(wd);
    return rc;
}

static int kt_parent_dir(const char* file, char* out, int cch)
{
    int i;
    int last = -1;
    if (!file || !out || cch <= 1) return KT_ERR;
    for (i = 0; file[i]; i++) {
        if (file[i] == '\\' || file[i] == '/') last = i;
    }
    if (last < 0) {
        out[0] = 0;
        return KT_OK;
    }
    if (last == 0) {
        out[0] = file[0];
        out[1] = 0;
        return KT_OK;
    }
    if (last + 1 > cch) return KT_ERR;
    memcpy(out, file, (size_t)last);
    out[last] = 0;
    return KT_OK;
}

static int kt_ensure_dir_for_file(const char* file)
{
    char dir[KT_PATH_MAX];
    if (kt_parent_dir(file, dir, KT_PATH_MAX) != KT_OK) return KT_ERR;
    if (!dir[0]) return KT_OK;
    return kt_ensure_dir(dir);
}

static int kt_join_path(const char* dir, const char* name, char* out, int cch)
{
    size_t dl;
    int n;
    if (!name || !out || cch <= 1) return KT_ERR;
    dl = dir ? strlen(dir) : 0;
    if (dl == 0) {
        n = snprintf(out, (size_t)cch, "%s", name);
    } else if (dir[dl - 1] == '\\' || dir[dl - 1] == '/') {
        n = snprintf(out, (size_t)cch, "%s%s", dir, name);
    } else {
        n = snprintf(out, (size_t)cch, "%s\\%s", dir, name);
    }
    if (n <= 0 || n >= cch) return KT_ERR;
    return KT_OK;
}

static int kt_clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int kt_ieq(const char* a, const char* b)
{
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int kt_ext_is(const char* path, const char* ext)
{
    const char* p;
    const char* dot = NULL;
    if (!path || !ext) return 0;
    for (p = path; *p; p++) {
        if (*p == '.') dot = p;
    }
    if (!dot) return 0;
    return kt_ieq(dot, ext);
}

static int kt_hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int kt_parse_color(const char* s, int* r, int* g, int* b)
{
    int i;
    if (!s || s[0] != '#' || strlen(s) != 7) return 0;
    for (i = 1; i < 7; i++) {
        if (kt_hexval(s[i]) < 0) return 0;
    }
    *r = kt_hexval(s[1]) * 16 + kt_hexval(s[2]);
    *g = kt_hexval(s[3]) * 16 + kt_hexval(s[4]);
    *b = kt_hexval(s[5]) * 16 + kt_hexval(s[6]);
    return 1;
}

static HRESULT kt_com_begin(int* owned)
{
    HRESULT hr;
    *owned = 0;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == S_OK || hr == S_FALSE) {
        *owned = 1;
        return S_OK;
    }
    if (hr == RPC_E_CHANGED_MODE) return S_OK;
    return hr;
}

static void kt_com_end(int owned)
{
    if (owned) CoUninitialize();
}

static HRESULT kt_factory_create(IWICImagingFactory** out)
{
    return CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)out);
}

static HRESULT kt_wrap_bgra(IWICImagingFactory* f, IWICBitmapSource* in, IWICBitmapSource** out)
{
    IWICFormatConverter* conv = NULL;
    HRESULT hr;
    if (!f || !in || !out) return E_INVALIDARG;
    hr = IWICImagingFactory_CreateFormatConverter(f, &conv);
    if (SUCCEEDED(hr)) {
        hr = IWICFormatConverter_Initialize(conv, in, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (FAILED(hr)) {
        if (conv) IWICFormatConverter_Release(conv);
        return hr;
    }
    *out = (IWICBitmapSource*)conv;
    return S_OK;
}

static HRESULT kt_decode_source(IWICImagingFactory* f, const char* path, int best_frame, IWICBitmapSource** out)
{
    wchar_t* wp = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* conv = NULL;
    HRESULT hr;
    if (!f || !path || !out) return E_INVALIDARG;
    wp = kt_utf8_to_wide(path);
    if (!wp) return E_INVALIDARG;
    hr = IWICImagingFactory_CreateDecoderFromFilename(f, wp, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (SUCCEEDED(hr) && best_frame) {
        UINT count = 0;
        UINT pick = 0;
        ULONGLONG area = 0;
        UINT i;
        if (SUCCEEDED(IWICBitmapDecoder_GetFrameCount(dec, &count))) {
            for (i = 0; i < count; i++) {
                IWICBitmapFrameDecode* cand = NULL;
                UINT cw = 0;
                UINT ch = 0;
                if (SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, i, &cand))) {
                    if (SUCCEEDED(IWICBitmapSource_GetSize((IWICBitmapSource*)cand, &cw, &ch))) {
                        ULONGLONG a = (ULONGLONG)cw * (ULONGLONG)ch;
                        if (a > area) {
                            area = a;
                            pick = i;
                        }
                    }
                    IWICBitmapFrameDecode_Release(cand);
                }
            }
        }
        hr = IWICBitmapDecoder_GetFrame(dec, pick, &frame);
    } else if (SUCCEEDED(hr)) {
        hr = IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    }
    if (SUCCEEDED(hr)) hr = kt_wrap_bgra(f, (IWICBitmapSource*)frame, &conv);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (dec) IWICBitmapDecoder_Release(dec);
    free(wp);
    if (FAILED(hr)) return hr;
    *out = conv;
    return S_OK;
}

static HRESULT kt_load_hicon_source(IWICImagingFactory* f, const char* path, int index, IWICBitmapSource** out)
{
    wchar_t* wp;
    HICON h = NULL;
    UINT n;
    IWICBitmap* bmp = NULL;
    IWICBitmapSource* conv = NULL;
    HRESULT hr;
    if (!f || !path || !out) return E_INVALIDARG;
    wp = kt_utf8_to_wide(path);
    if (!wp) return E_INVALIDARG;
    n = ExtractIconExW(wp, index, &h, NULL, 1);
    free(wp);
    if (n == 0 || h == NULL) return E_FAIL;
    hr = IWICImagingFactory_CreateBitmapFromHICON(f, h, &bmp);
    DestroyIcon(h);
    if (FAILED(hr)) return hr;
    hr = kt_wrap_bgra(f, (IWICBitmapSource*)bmp, &conv);
    IWICBitmap_Release(bmp);
    if (FAILED(hr)) return hr;
    *out = conv;
    return S_OK;
}

static HRESULT kt_scale_source(IWICImagingFactory* f, IWICBitmapSource* src, UINT w, UINT h, IWICBitmapSource** out)
{
    WICBitmapInterpolationMode modes[3];
    IWICBitmapScaler* scaler = NULL;
    HRESULT hr = E_FAIL;
    int i;
    if (!f || !src || !out || w == 0 || h == 0) return E_INVALIDARG;
    modes[0] = WICBitmapInterpolationModeFant;
    modes[1] = WICBitmapInterpolationModeCubic;
    modes[2] = WICBitmapInterpolationModeLinear;
    for (i = 0; i < 3; i++) {
        scaler = NULL;
        hr = IWICImagingFactory_CreateBitmapScaler(f, &scaler);
        if (FAILED(hr)) return hr;
        hr = IWICBitmapScaler_Initialize(scaler, src, w, h, modes[i]);
        if (SUCCEEDED(hr)) break;
        IWICBitmapScaler_Release(scaler);
        scaler = NULL;
    }
    if (!scaler) return hr;
    *out = (IWICBitmapSource*)scaler;
    return S_OK;
}

static int kt_read_bgra(IWICBitmapSource* src, UINT* pw, UINT* ph, BYTE** pbuf)
{
    UINT w = 0;
    UINT h = 0;
    UINT stride;
    BYTE* buf;
    HRESULT hr;
    if (!src || !pw || !ph || !pbuf) return KT_ERR;
    hr = IWICBitmapSource_GetSize(src, &w, &h);
    if (FAILED(hr) || w == 0 || h == 0) return KT_ERR;
    stride = w * 4;
    buf = (BYTE*)malloc((size_t)stride * (size_t)h);
    if (!buf) return KT_ERR;
    hr = IWICBitmapSource_CopyPixels(src, NULL, stride, stride * h, buf);
    if (FAILED(hr)) {
        free(buf);
        return KT_ERR;
    }
    *pw = w;
    *ph = h;
    *pbuf = buf;
    return KT_OK;
}

static HRESULT kt_write_png_w(IWICImagingFactory* f, IWICBitmapSource* src, const wchar_t* path)
{
    IWICStream* stream = NULL;
    IWICBitmapEncoder* enc = NULL;
    IWICBitmapFrameEncode* frame = NULL;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    UINT w = 0;
    UINT h = 0;
    HRESULT hr;
    hr = IWICImagingFactory_CreateStream(f, &stream);
    if (SUCCEEDED(hr)) hr = IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = IWICImagingFactory_CreateEncoder(f, &GUID_ContainerFormatPng, NULL, &enc);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Initialize(enc, (IStream*)stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_CreateNewFrame(enc, &frame, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapSource_GetSize(src, &w, &h);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetSize(frame, w, h);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WriteSource(frame, src, NULL);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_Commit(frame);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(enc);
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (enc) IWICBitmapEncoder_Release(enc);
    if (stream) IWICStream_Release(stream);
    return hr;
}

static int kt_save_png_source(IWICImagingFactory* f, IWICBitmapSource* src, const char* dst)
{
    char abspath[KT_PATH_MAX];
    char tmppath[KT_PATH_MAX + 64];
    wchar_t* wdst = NULL;
    wchar_t* wtmp = NULL;
    HRESULT hr;
    int rc = KT_ERR;
    int n;
    if (!f || !src || !dst || !dst[0]) return KT_ERR;
    if (kt_abs_path_utf8(dst, abspath, KT_PATH_MAX) != KT_OK) return KT_ERR;
    if (kt_ensure_dir_for_file(abspath) != KT_OK) return KT_ERR;
    n = snprintf(tmppath, sizeof(tmppath), "%s%s", abspath, KT_TMP_SUFFIX_A);
    if (n <= 0 || n >= (int)sizeof(tmppath)) return KT_ERR;
    wdst = kt_utf8_to_wide(abspath);
    wtmp = kt_utf8_to_wide(tmppath);
    if (wdst && wtmp) {
        DeleteFileW(wtmp);
        hr = kt_write_png_w(f, src, wtmp);
        if (SUCCEEDED(hr)) {
            if (MoveFileExW(wtmp, wdst, MOVEFILE_REPLACE_EXISTING)) rc = KT_OK;
        }
    }
    if (rc != KT_OK && wtmp) DeleteFileW(wtmp);
    free(wtmp);
    free(wdst);
    return rc;
}

static int kt_save_bgra_png(IWICImagingFactory* f, const BYTE* buf, int w, int h, const char* dst)
{
    IWICBitmap* bmp = NULL;
    HRESULT hr;
    int rc;
    if (!buf || w <= 0 || h <= 0) return KT_ERR;
    hr = IWICImagingFactory_CreateBitmapFromMemory(f, (UINT)w, (UINT)h, &GUID_WICPixelFormat32bppBGRA, (UINT)(w * 4), (UINT)(w * 4 * h), (BYTE*)buf, &bmp);
    if (FAILED(hr)) return KT_ERR;
    rc = kt_save_png_source(f, (IWICBitmapSource*)bmp, dst);
    IWICBitmap_Release(bmp);
    return rc;
}

static void kt_fill_bgra(BYTE* buf, int w, int h, int r, int g, int b, int a)
{
    int i;
    int n = w * h;
    for (i = 0; i < n; i++) {
        buf[i * 4 + 0] = (BYTE)b;
        buf[i * 4 + 1] = (BYTE)g;
        buf[i * 4 + 2] = (BYTE)r;
        buf[i * 4 + 3] = (BYTE)a;
    }
}

static void kt_blit_bgra(BYTE* canvas, int cw, int ch, const BYTE* src, int sw, int sh, int ox, int oy)
{
    int y;
    int x;
    for (y = 0; y < sh; y++) {
        int dy = oy + y;
        if (dy < 0 || dy >= ch) continue;
        for (x = 0; x < sw; x++) {
            const BYTE* s;
            BYTE* d;
            int dx = ox + x;
            unsigned int a;
            if (dx < 0 || dx >= cw) continue;
            s = src + ((size_t)y * (size_t)sw + (size_t)x) * 4;
            a = s[3];
            if (a == 0) continue;
            d = canvas + ((size_t)dy * (size_t)cw + (size_t)dx) * 4;
            if (a >= 255) {
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
            } else {
                unsigned int ia = 255 - a;
                d[0] = (BYTE)((s[0] * a + d[0] * ia) / 255);
                d[1] = (BYTE)((s[1] * a + d[1] * ia) / 255);
                d[2] = (BYTE)((s[2] * a + d[2] * ia) / 255);
            }
            d[3] = 255;
        }
    }
}

static int kt_cfg_str(KtConfig* cfg, const char* id, const char* key, char* buf, int cch)
{
    if (!buf || cch <= 0) return 0;
    buf[0] = 0;
    if (kt_config_get(cfg, id, key, buf, cch) != KT_OK) {
        buf[0] = 0;
        return 0;
    }
    return buf[0] ? 1 : 0;
}

static int kt_render_size(int size, int* w, int* h)
{
    int sw = 0;
    int sh = 0;
    if (w == NULL || h == NULL) return KT_ERR;
    if (kt_size_pixels(size, &sw, &sh) == KT_OK && sw > 0 && sh > 0) {
        *w = sw;
        *h = sh;
        return KT_OK;
    }
    if (size == KT_SIZE_SMALL) {
        *w = 70;
        *h = 70;
        return KT_OK;
    }
    if (size == KT_SIZE_MEDIUM) {
        *w = 150;
        *h = 150;
        return KT_OK;
    }
    if (size == KT_SIZE_WIDE) {
        *w = 310;
        *h = 150;
        return KT_OK;
    }
    if (size == KT_SIZE_LARGE) {
        *w = 310;
        *h = 310;
        return KT_OK;
    }
    return KT_ERR;
}

static int kt_cell_name(const char* target, int cw, int ch, int total, int seq, char* out, int cch)
{
    int n;
    if (total <= 1) {
        n = snprintf(out, (size_t)cch, "%s_%dx%d.png", target, cw, ch);
    } else {
        n = snprintf(out, (size_t)cch, "%s_%dx%d_%d.png", target, cw, ch, seq);
    }
    if (n <= 0 || n >= cch) return KT_ERR;
    return KT_OK;
}

static int kt_load_icon_source(IWICImagingFactory* f, const char* path, int index, IWICBitmapSource** out)
{
    HRESULT hr;
    int is_ico;
    if (kt_ext_is(path, ".exe") || kt_ext_is(path, ".dll")) {
        return SUCCEEDED(kt_load_hicon_source(f, path, index, out)) ? KT_OK : KT_ERR;
    }
    is_ico = (kt_ext_is(path, ".ico") || kt_ext_is(path, ".icl")) ? 1 : 0;
    hr = kt_decode_source(f, path, is_ico, out);
    if (SUCCEEDED(hr)) return KT_OK;
    if (is_ico) {
        return SUCCEEDED(kt_load_hicon_source(f, path, index, out)) ? KT_OK : KT_ERR;
    }
    return KT_ERR;
}

int kt_image_info(const char* path, int* width, int* height)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapDecoder* dec = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    wchar_t* wp = NULL;
    HRESULT hr;
    UINT w = 0;
    UINT h = 0;
    int rc = KT_ERR;
    if (!path || !width || !height) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    wp = kt_utf8_to_wide(path);
    if (wp) {
        hr = IWICImagingFactory_CreateDecoderFromFilename(f, wp, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
        if (SUCCEEDED(hr)) hr = IWICBitmapDecoder_GetFrame(dec, 0, &frame);
        if (SUCCEEDED(hr)) hr = IWICBitmapSource_GetSize((IWICBitmapSource*)frame, &w, &h);
        if (SUCCEEDED(hr) && w > 0 && h > 0) {
            *width = (int)w;
            *height = (int)h;
            rc = KT_OK;
        }
        free(wp);
    }
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (dec) IWICBitmapDecoder_Release(dec);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}

int kt_image_resize(const char* src, int dst_w, int dst_h, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapSource* in = NULL;
    IWICBitmapSource* scaled = NULL;
    int rc = KT_ERR;
    if (!src || !dst || dst_w <= 0 || dst_h <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    if (SUCCEEDED(kt_decode_source(f, src, 0, &in))) {
        if (SUCCEEDED(kt_scale_source(f, in, (UINT)dst_w, (UINT)dst_h, &scaled))) {
            rc = kt_save_png_source(f, scaled, dst);
            IWICBitmapSource_Release(scaled);
        }
        IWICBitmapSource_Release(in);
    }
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}

int kt_image_cut_grid(const char* src, int rows, int cols, int cell_w, int cell_h, const char* target_name, const char* out_dir, int* out_count)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapSource* in = NULL;
    UINT sw = 0;
    UINT sh = 0;
    int total;
    int seq = 0;
    int made = 0;
    int r;
    int c;
    char target[512];
    char name[512];
    char full[KT_PATH_MAX];
    int rc = KT_ERR;
    if (!src || !out_dir || !out_dir[0] || rows <= 0 || cols <= 0 || cell_w <= 0 || cell_h <= 0 || !out_count) return KT_ERR;
    total = rows * cols;
    if (target_name && target_name[0]) {
        snprintf(target, sizeof(target), "%s", target_name);
    } else {
        snprintf(target, sizeof(target), "%s", "tile");
    }
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    if (kt_ensure_dir(out_dir) != KT_OK) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    if (SUCCEEDED(kt_decode_source(f, src, 0, &in))) {
        if (SUCCEEDED(IWICBitmapSource_GetSize(in, &sw, &sh)) && sw > 0 && sh > 0 && (UINT)rows <= sh && (UINT)cols <= sw) {
            rc = KT_OK;
            for (r = 0; r < rows && rc == KT_OK; r++) {
                int y0 = (int)((sh * (UINT)r) / (UINT)rows);
                int y1 = (int)((sh * (UINT)(r + 1)) / (UINT)rows);
                for (c = 0; c < cols && rc == KT_OK; c++) {
                    int x0 = (int)((sw * (UINT)c) / (UINT)cols);
                    int x1 = (int)((sw * (UINT)(c + 1)) / (UINT)cols);
                    WICRect box;
                    IWICBitmapClipper* clip = NULL;
                    IWICBitmapSource* cell = NULL;
                    IWICBitmapSource* scaled = NULL;
                    box.X = x0;
                    box.Y = y0;
                    box.Width = x1 - x0;
                    box.Height = y1 - y0;
                    seq++;
                    if (box.Width <= 0 || box.Height <= 0) {
                        rc = KT_ERR;
                        break;
                    }
                    if (kt_cell_name(target, cell_w, cell_h, total, seq, name, (int)sizeof(name)) != KT_OK) {
                        rc = KT_ERR;
                        break;
                    }
                    if (kt_join_path(out_dir, name, full, KT_PATH_MAX) != KT_OK) {
                        rc = KT_ERR;
                        break;
                    }
                    if (FAILED(IWICImagingFactory_CreateBitmapClipper(f, &clip))) {
                        rc = KT_ERR;
                        break;
                    }
                    if (FAILED(IWICBitmapClipper_Initialize(clip, in, &box))) {
                        IWICBitmapClipper_Release(clip);
                        rc = KT_ERR;
                        break;
                    }
                    cell = (IWICBitmapSource*)clip;
                    if (SUCCEEDED(kt_scale_source(f, cell, (UINT)cell_w, (UINT)cell_h, &scaled))) {
                        if (kt_save_png_source(f, scaled, full) != KT_OK) {
                            rc = KT_ERR;
                        } else {
                            made++;
                        }
                        IWICBitmapSource_Release(scaled);
                    } else {
                        rc = KT_ERR;
                    }
                    IWICBitmapClipper_Release(clip);
                }
            }
            if (rc == KT_OK) {
                *out_count = made;
            } else {
                int j;
                for (j = 1; j <= made; j++) {
                    if (kt_cell_name(target, cell_w, cell_h, total, j, name, (int)sizeof(name)) == KT_OK) {
                        if (kt_join_path(out_dir, name, full, KT_PATH_MAX) == KT_OK) {
                            wchar_t* wfull = kt_utf8_to_wide(full);
                            if (wfull) {
                                DeleteFileW(wfull);
                                free(wfull);
                            }
                        }
                    }
                }
            }
        }
        IWICBitmapSource_Release(in);
    }
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}

static int kt_editor_icon_size(int size)
{
    if (size == KT_SIZE_SMALL) {
        return 24;
    }
    if (size == KT_SIZE_LARGE) {
        return 64;
    }
    return 32;
}

static void kt_editor_tile_size(int size, int* w, int* h)
{
    if (size == KT_SIZE_SMALL) {
        *w = 48;
        *h = 48;
    } else if (size == KT_SIZE_MEDIUM) {
        *w = 100;
        *h = 100;
    } else if (size == KT_SIZE_WIDE) {
        *w = 204;
        *h = 100;
    } else {
        *w = 204;
        *h = 204;
    }
}
int kt_image_render_tile(KtConfig* cfg, const char* id, int size, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    int w = 0;
    int h = 0;
    int ew = 1;
    int eh = 1;
    int tile_size = 0;
    BYTE* canvas = NULL;
    char color[64];
    char imgpath[KT_PATH_MAX];
    char iconpath[KT_PATH_MAX];
    int rc = KT_ERR;
    if (!cfg || !id || !dst || !dst[0]) return KT_ERR;
    if (kt_render_size(size, &w, &h) != KT_OK) return KT_ERR;
    tile_size = kt_clamp_int(kt_config_get_int(cfg, id, "size", 1), 0, 3);
    kt_editor_tile_size(tile_size, &ew, &eh);
    if (ew < 1) ew = 1;
    if (eh < 1) eh = 1;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    canvas = (BYTE*)malloc((size_t)w * (size_t)h * 4);
    if (!canvas) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    memset(canvas, 0, (size_t)w * (size_t)h * 4);
    {
        IWICBitmapSource* img = NULL;
        int has_img = 0;
        if (kt_cfg_str(cfg, id, "image", imgpath, (int)sizeof(imgpath))) {
            if (SUCCEEDED(kt_decode_source(f, imgpath, 0, &img))) has_img = 1;
        }
        if (kt_config_get_int(cfg, id, "colorEnabled", 0) == 1) {
            if (kt_cfg_str(cfg, id, "color", color, (int)sizeof(color))) {
                int r = 0;
                int g = 0;
                int b = 0;
                if (kt_parse_color(color, &r, &g, &b)) {
                    kt_fill_bgra(canvas, w, h, r, g, b, 255);
                }
            }
        } else if (!has_img) {
            int r = 0;
            int g = 0;
            int b = 0;
            if (kt_parse_color(KT_DEFAULT_TILE_COLOR, &r, &g, &b)) {
                kt_fill_bgra(canvas, w, h, r, g, b, 255);
            }
        }
        if (has_img) {
            UINT iw = 0;
            UINT ih = 0;
            if (SUCCEEDED(IWICBitmapSource_GetSize(img, &iw, &ih)) && iw > 0 && ih > 0) {
                int pct = kt_clamp_int(kt_config_get_int(cfg, id, "imageScale", 100), 10, 400);
                UINT tw;
                UINT th;
                tw = (UINT)(((ULONGLONG)iw * (ULONGLONG)(UINT)pct * (ULONGLONG)(UINT)w) / (100ULL * (ULONGLONG)(UINT)ew));
                th = (UINT)(((ULONGLONG)ih * (ULONGLONG)(UINT)pct * (ULONGLONG)(UINT)h) / (100ULL * (ULONGLONG)(UINT)eh));
                IWICBitmapSource* scaled = NULL;
                if (tw == 0) tw = 1;
                if (th == 0) th = 1;
                if (SUCCEEDED(kt_scale_source(f, img, tw, th, &scaled))) {
                    UINT bw = 0;
                    UINT bh = 0;
                    BYTE* buf = NULL;
                    if (kt_read_bgra(scaled, &bw, &bh, &buf) == KT_OK) {
                        int ox = ((w - (int)bw) / 2) + (int)(((LONGLONG)kt_config_get_int(cfg, id, "imageOffsetX", 0) * (LONGLONG)w) / (LONGLONG)ew);
                        int oy = ((h - (int)bh) / 2) + (int)(((LONGLONG)kt_config_get_int(cfg, id, "imageOffsetY", 0) * (LONGLONG)h) / (LONGLONG)eh);
                        kt_blit_bgra(canvas, w, h, buf, (int)bw, (int)bh, ox, oy);
                        free(buf);
                    }
                    IWICBitmapSource_Release(scaled);
                }
            }
        }
        if (img) IWICBitmapSource_Release(img);
    }
    if (kt_config_get_int(cfg, id, "iconEnabled", 0) == 1) {
        if (kt_cfg_str(cfg, id, "iconPath", iconpath, (int)sizeof(iconpath))) {
            IWICBitmapSource* icon = NULL;
            if (kt_load_icon_source(f, iconpath, 0, &icon) == KT_OK) {
                int isize = kt_editor_icon_size(tile_size);
                int isize_w = (int)(((LONGLONG)isize * (LONGLONG)w) / (LONGLONG)ew);
                int isize_h = (int)(((LONGLONG)isize * (LONGLONG)h) / (LONGLONG)eh);
                if (isize_w < 1) isize_w = 1;
                if (isize_h < 1) isize_h = 1;
                IWICBitmapSource* scaled = NULL;
                if (SUCCEEDED(kt_scale_source(f, icon, (UINT)isize_w, (UINT)isize_h, &scaled))) {
                    UINT bw = 0;
                    UINT bh = 0;
                    BYTE* buf = NULL;
                    if (kt_read_bgra(scaled, &bw, &bh, &buf) == KT_OK) {
                        int ox = ((w - (int)bw) / 2) + (int)(((LONGLONG)kt_config_get_int(cfg, id, "iconOffsetX", 0) * (LONGLONG)w) / (LONGLONG)ew);
                        int oy = ((h - (int)bh) / 2) + (int)(((LONGLONG)kt_config_get_int(cfg, id, "iconOffsetY", 0) * (LONGLONG)h) / (LONGLONG)eh) - (int)(((LONGLONG)2 * (LONGLONG)h) / (LONGLONG)eh);
                        kt_blit_bgra(canvas, w, h, buf, (int)bw, (int)bh, ox, oy);
                        free(buf);
                    }
                    IWICBitmapSource_Release(scaled);
                }
                IWICBitmapSource_Release(icon);
            }
        }
    }
    rc = kt_save_bgra_png(f, canvas, w, h, dst);
    free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}


static void kt_blend_px(BYTE* canvas, int w, int h, int x, int y, int r, int g, int b, int alpha)
{
    BYTE* p;
    if (!canvas || x < 0 || y < 0 || x >= w || y >= h || alpha <= 0) return;
    p = canvas + ((size_t)y * (size_t)w + (size_t)x) * 4;
    p[0] = (BYTE)((b * alpha + p[0] * (255 - alpha)) / 255);
    p[1] = (BYTE)((g * alpha + p[1] * (255 - alpha)) / 255);
    p[2] = (BYTE)((r * alpha + p[2] * (255 - alpha)) / 255);
    p[3] = 255;
}

static int kt_text_coverage(const wchar_t* text, int font_px, int cw, int ch, int left, int top, BYTE* out)
{
    HDC screen;
    HDC mem;
    BITMAPINFO bi;
    void* bits = NULL;
    HBITMAP dib;
    HGDIOBJ old_bmp;
    HFONT font;
    HGDIOBJ old_font;
    LOGFONTW lf;
    int ok = 0;
    if (!text || !out || cw <= 0 || ch <= 0 || font_px <= 0) return 0;
    screen = GetDC(NULL);
    if (!screen) return 0;
    mem = CreateCompatibleDC(screen);
    if (!mem)
    {
        ReleaseDC(NULL, screen);
        return 0;
    }
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cw;
    bi.bmiHeader.biHeight = -ch;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dib && bits)
    {
        old_bmp = SelectObject(mem, dib);
        memset(bits, 0, (size_t)cw * (size_t)ch * 4);
        ZeroMemory(&lf, sizeof(lf));
        lf.lfHeight = -font_px;
        lf.lfWeight = 600;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        lstrcpynW(lf.lfFaceName, L"Segoe UI", 32);
        font = CreateFontIndirectW(&lf);
        if (font)
        {
            old_font = SelectObject(mem, font);
            SetBkMode(mem, OPAQUE);
            SetBkColor(mem, RGB(0, 0, 0));
            SetTextColor(mem, RGB(255, 255, 255));
            TextOutW(mem, left, top, text, (int)wcslen(text));
            SelectObject(mem, old_font);
            DeleteObject(font);
            memcpy(out, bits, (size_t)cw * (size_t)ch * 4);
            ok = 1;
        }
        SelectObject(mem, old_bmp);
        DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    return ok;
}

int kt_image_render_clear(int w, int h, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    BYTE* canvas = NULL;
    int rc = KT_ERR;
    if (!dst || !dst[0] || w <= 0 || h <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    canvas = (BYTE*)malloc((size_t)w * (size_t)h * 4);
    if (!canvas) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    memset(canvas, 0, (size_t)w * (size_t)h * 4);
    rc = kt_save_bgra_png(f, canvas, w, h, dst);
    free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}

int kt_image_render_fill(int w, int h, const char* color, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    BYTE* canvas = NULL;
    int r = 0;
    int g = 0;
    int b = 0;
    int rc = KT_ERR;
    if (!dst || !dst[0] || w <= 0 || h <= 0 || !color) return KT_ERR;
    if (!kt_parse_color(color, &r, &g, &b)) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    canvas = (BYTE*)malloc((size_t)w * (size_t)h * 4);
    if (!canvas) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    kt_fill_bgra(canvas, w, h, r, g, b, 255);
    rc = kt_save_bgra_png(f, canvas, w, h, dst);
    free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}
int kt_image_add_label(const char* src, const char* label, int font_px, int margin, int color, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapSource* dec = NULL;
    BYTE* canvas = NULL;
    BYTE* mask = NULL;
    wchar_t* text = NULL;
    UINT w = 0;
    UINT h = 0;
    int rc = KT_ERR;
    int x;
    int y;
    int r;
    int g;
    int b;
    if (!src || !src[0] || !label || !label[0] || !dst || !dst[0] || font_px <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f)))
    {
        kt_com_end(owned);
        return KT_ERR;
    }
    if (SUCCEEDED(kt_decode_source(f, src, 0, &dec)) && kt_read_bgra(dec, &w, &h, &canvas) == KT_OK && w > 0 && h > 0)
    {
        IWICBitmapSource_Release(dec);
        dec = NULL;
        mask = (BYTE*)calloc((size_t)w * (size_t)h, 4);
        text = kt_utf8_to_wide(label);
        if (mask && text)
        {
            int top = (int)h - margin - font_px;
            int left = margin;
            if (top < 0) top = 0;
            if (left < 0) left = 0;
            if (kt_text_coverage(text, font_px, (int)w, (int)h, left, top, mask))
            {
                r = (color >> 16) & 0xFF;
                g = (color >> 8) & 0xFF;
                b = color & 0xFF;
                for (y = 0; y < (int)h; y++)
                {
                    for (x = 0; x < (int)w; x++)
                    {
                        BYTE* m = mask + ((size_t)y * (size_t)w + (size_t)x) * 4;
                        int cover = (m[0] + m[1] + m[2]) / 3;
                        if (cover <= 0) continue;
                        kt_blend_px(canvas, (int)w, (int)h, x + 2, y + 2, 0, 0, 0, cover / 2);
                        kt_blend_px(canvas, (int)w, (int)h, x, y, r, g, b, cover);
                    }
                }
                rc = kt_save_bgra_png(f, canvas, (int)w, (int)h, dst);
            }
        }
        if (text) free(text);
        if (mask) free(mask);
    }
    if (dec) IWICBitmapSource_Release(dec);
    if (canvas) free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}
static void kt_text_bounds(const BYTE* mask, int w, int h, int* x0, int* y0, int* x1, int* y1)
{
    int x;
    int y;
    *x0 = w;
    *y0 = h;
    *x1 = -1;
    *y1 = -1;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            const BYTE* m = mask + ((size_t)y * (size_t)w + (size_t)x) * 4;
            if (((m[0] + m[1] + m[2]) / 3) <= 0) continue;
            if (x < *x0) *x0 = x;
            if (x > *x1) *x1 = x;
            if (y < *y0) *y0 = y;
            if (y > *y1) *y1 = y;
        }
    }
}

int kt_image_add_label_box(const char* src, const char* line1, const char* line2, int font_px, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapSource* dec = NULL;
    BYTE* canvas = NULL;
    BYTE* mask = NULL;
    wchar_t* text = NULL;
    UINT w = 0;
    UINT h = 0;
    int rc = KT_ERR;
    if (!src || !src[0] || !line1 || !line1[0] || !dst || !dst[0] || font_px <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f)))
    {
        kt_com_end(owned);
        return KT_ERR;
    }
    if (SUCCEEDED(kt_decode_source(f, src, 0, &dec)) && kt_read_bgra(dec, &w, &h, &canvas) == KT_OK && w > 0 && h > 0)
    {
        IWICBitmapSource_Release(dec);
        dec = NULL;
        mask = (BYTE*)calloc((size_t)w * (size_t)h, 4);
        if (mask)
        {
            int a0;
            int b0;
            int a1;
            int b1;
            int c0;
            int d0;
            int c1;
            int d1;
            int pad = font_px / 3;
            int height1 = 0;
            int height2 = 0;
            int box_w;
            int box_h;
            int left;
            int top;
            int x;
            int y;
            if (pad < 2) pad = 2;
            text = kt_utf8_to_wide(line1);
            if (text && kt_text_coverage(text, font_px, (int)w, (int)h, 0, 0, mask))
            {
                kt_text_bounds(mask, (int)w, (int)h, &a0, &b0, &a1, &b1);
            }
            else
            {
                a0 = 0;
                b0 = 0;
                a1 = -1;
                b1 = -1;
            }
            if (text) free(text);
            text = NULL;
            if (a1 < a0 || b1 < b0) goto done;
            height1 = b1 - b0 + 1;
            memset(mask, 0, (size_t)w * (size_t)h * 4);
            text = kt_utf8_to_wide(line2);
            if (text && kt_text_coverage(text, font_px, (int)w, (int)h, 0, 0, mask))
            {
                kt_text_bounds(mask, (int)w, (int)h, &c0, &d0, &c1, &d1);
            }
            else
            {
                c0 = 0;
                d0 = 0;
                c1 = -1;
                d1 = -1;
            }
            if (text) free(text);
            text = NULL;
            if (c1 < c0 || d1 < d0) goto done;
            height2 = d1 - d0 + 1;
            box_w = (int)w;
            box_h = (int)h;
            left = 0;
            top = 0;
            for (y = top; y < top + box_h && y < (int)h; y++)
            {
                for (x = left; x < left + box_w && x < (int)w; x++)
                {
                    BYTE* p = canvas + ((size_t)y * (size_t)w + (size_t)x) * 4;
                    p[0] = 255;
                    p[1] = 255;
                    p[2] = 255;
                    p[3] = 255;
                }
            }
            memset(mask, 0, (size_t)w * (size_t)h * 4);
            text = kt_utf8_to_wide(line1);
            if (text && kt_text_coverage(text, font_px, (int)w, (int)h, 0, 0, mask))
            {
                int dx = left + ((box_w - (a1 - a0 + 1)) / 2) - a0;
                int dy = ((int)h - (height1 + height2 + pad)) / 2 - b0;
                for (y = 0; y < (int)h; y++)
                {
                    for (x = 0; x < (int)w; x++)
                    {
                        const BYTE* m = mask + ((size_t)y * (size_t)w + (size_t)x) * 4;
                        int cover = (m[0] + m[1] + m[2]) / 3;
                        if (cover <= 0) continue;
                        kt_blend_px(canvas, (int)w, (int)h, x + dx, y + dy, 0, 0, 0, cover);
                    }
                }
            }
            if (text) free(text);
            text = NULL;
            memset(mask, 0, (size_t)w * (size_t)h * 4);
            text = kt_utf8_to_wide(line2);
            if (text && kt_text_coverage(text, font_px, (int)w, (int)h, 0, 0, mask))
            {
                int dx = left + ((box_w - (c1 - c0 + 1)) / 2) - c0;
                int dy = ((int)h - (height1 + height2 + pad)) / 2 + height1 + pad - d0;
                for (y = 0; y < (int)h; y++)
                {
                    for (x = 0; x < (int)w; x++)
                    {
                        const BYTE* m = mask + ((size_t)y * (size_t)w + (size_t)x) * 4;
                        int cover = (m[0] + m[1] + m[2]) / 3;
                        if (cover <= 0) continue;
                        kt_blend_px(canvas, (int)w, (int)h, x + dx, y + dy, 0, 0, 0, cover);
                    }
                }
            }
            rc = kt_save_bgra_png(f, canvas, (int)w, (int)h, dst);
        }
    }
done:
    if (text) free(text);
    if (mask) free(mask);
    if (dec) IWICBitmapSource_Release(dec);
    if (canvas) free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}
int kt_image_render_slice(const char* src, int canvas_w, int canvas_h, int image_w, int image_h, int offset_x, int offset_y, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    BYTE* canvas = NULL;
    int rc = KT_ERR;
    if (!src || !src[0] || !dst || !dst[0]) return KT_ERR;
    if (canvas_w <= 0 || canvas_h <= 0 || image_w <= 0 || image_h <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    canvas = (BYTE*)malloc((size_t)canvas_w * (size_t)canvas_h * 4);
    if (!canvas) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    memset(canvas, 0, (size_t)canvas_w * (size_t)canvas_h * 4);
    {
        IWICBitmapSource* img = NULL;
        if (SUCCEEDED(kt_decode_source(f, src, 0, &img))) {
            IWICBitmapSource* scaled = NULL;
            if (SUCCEEDED(kt_scale_source(f, img, (UINT)image_w, (UINT)image_h, &scaled))) {
                UINT bw = 0;
                UINT bh = 0;
                BYTE* buf = NULL;
                if (kt_read_bgra(scaled, &bw, &bh, &buf) == KT_OK) {
                    kt_blit_bgra(canvas, canvas_w, canvas_h, buf, (int)bw, (int)bh, offset_x, offset_y);
                    free(buf);
                }
                IWICBitmapSource_Release(scaled);
            }
            IWICBitmapSource_Release(img);
        }
    }
    rc = kt_save_bgra_png(f, canvas, canvas_w, canvas_h, dst);
    free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}
int kt_image_extract_icon(const char* src, int index, int icon_size, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    IWICBitmapSource* in = NULL;
    IWICBitmapSource* scaled = NULL;
    int want;
    int rc = KT_ERR;
    if (!src || !src[0] || !dst || !dst[0] || icon_size <= 0) return KT_ERR;
    if (index < 0) index = 0;
    want = kt_clamp_int(icon_size, 1, 4096);
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    if (kt_load_icon_source(f, src, index, &in) == KT_OK) {
        if (SUCCEEDED(kt_scale_source(f, in, (UINT)want, (UINT)want, &scaled))) {
            rc = kt_save_png_source(f, scaled, dst);
            IWICBitmapSource_Release(scaled);
        }
        IWICBitmapSource_Release(in);
    }
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}

int kt_image_composite(const char* background, const char* icon, int icon_size, int offset_x, int offset_y, int dst_w, int dst_h, const char* dst)
{
    int owned = 0;
    IWICImagingFactory* f = NULL;
    BYTE* canvas = NULL;
    int rc = KT_ERR;
    if (!dst || !dst[0] || dst_w <= 0 || dst_h <= 0) return KT_ERR;
    if (kt_com_begin(&owned) != S_OK) return KT_ERR;
    if (FAILED(kt_factory_create(&f))) {
        kt_com_end(owned);
        return KT_ERR;
    }
    canvas = (BYTE*)malloc((size_t)dst_w * (size_t)dst_h * 4);
    if (!canvas) {
        IWICImagingFactory_Release(f);
        kt_com_end(owned);
        return KT_ERR;
    }
    memset(canvas, 0, (size_t)dst_w * (size_t)dst_h * 4);
    rc = KT_OK;
    if (background && background[0]) {
        IWICBitmapSource* bg = NULL;
        if (SUCCEEDED(kt_decode_source(f, background, 0, &bg))) {
            IWICBitmapSource* scaled = NULL;
            if (SUCCEEDED(kt_scale_source(f, bg, (UINT)dst_w, (UINT)dst_h, &scaled))) {
                UINT bw = 0;
                UINT bh = 0;
                BYTE* buf = NULL;
                if (kt_read_bgra(scaled, &bw, &bh, &buf) == KT_OK) {
                    kt_blit_bgra(canvas, dst_w, dst_h, buf, (int)bw, (int)bh, 0, 0);
                    free(buf);
                } else {
                    rc = KT_ERR;
                }
                IWICBitmapSource_Release(scaled);
            } else {
                rc = KT_ERR;
            }
            IWICBitmapSource_Release(bg);
        } else {
            rc = KT_ERR;
        }
    }
    if (rc == KT_OK && icon && icon[0] && icon_size > 0) {
        IWICBitmapSource* ic = NULL;
        int want = kt_clamp_int(icon_size, 1, 4096);
        if (kt_load_icon_source(f, icon, 0, &ic) == KT_OK) {
            IWICBitmapSource* scaled = NULL;
            if (SUCCEEDED(kt_scale_source(f, ic, (UINT)want, (UINT)want, &scaled))) {
                UINT bw = 0;
                UINT bh = 0;
                BYTE* buf = NULL;
                if (kt_read_bgra(scaled, &bw, &bh, &buf) == KT_OK) {
                    int ox = ((dst_w - (int)bw) / 2) + offset_x;
                    int oy = ((dst_h - (int)bh) / 2) + offset_y;
                    kt_blit_bgra(canvas, dst_w, dst_h, buf, (int)bw, (int)bh, ox, oy);
                    free(buf);
                } else {
                    rc = KT_ERR;
                }
                IWICBitmapSource_Release(scaled);
            } else {
                rc = KT_ERR;
            }
            IWICBitmapSource_Release(ic);
        } else {
            rc = KT_ERR;
        }
    }
    if (rc == KT_OK) {
        rc = kt_save_bgra_png(f, canvas, dst_w, dst_h, dst);
    }
    free(canvas);
    IWICImagingFactory_Release(f);
    kt_com_end(owned);
    return rc;
}
