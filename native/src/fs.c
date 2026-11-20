#include "koitile_internal.h"

#define KT_MAX_LONG_PATH 32767

static int kt_sep(char c)
{
    return c == '\\' || c == '/';
}

static int kt_file_exists_w(const wchar_t* path)
{
    DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES;
}

static char* kt_project_data_root(void)
{
    wchar_t exe[KT_MAX_LONG_PATH];
    DWORD n;
    int i;
    int cut;
    size_t len;
    n = GetModuleFileNameW(NULL, exe, KT_MAX_LONG_PATH);
    if (n == 0 || n >= (DWORD)KT_MAX_LONG_PATH)
    {
        return NULL;
    }
    cut = -1;
    for (i = (int)n - 1; i >= 0; i--)
    {
        if (kt_sep((char)exe[i]))
        {
            cut = i;
            break;
        }
    }
    if (cut < 0)
    {
        return NULL;
    }
    exe[cut] = L'\0';
    for (i = 0; i < 10; i++)
    {
        wchar_t probe[KT_MAX_LONG_PATH];
        wchar_t data[KT_MAX_LONG_PATH];
        lstrcpynW(probe, exe, KT_MAX_LONG_PATH);
        lstrcatW(probe, L"\\koiTile.sln");
        if (kt_file_exists_w(probe))
        {
            lstrcpynW(data, exe, KT_MAX_LONG_PATH);
            lstrcatW(data, L"\\data");
            return kt_utf16_to_utf8(data);
        }
        len = lstrlenW(exe);
        while (len > 0)
        {
            len--;
            if (kt_sep((char)exe[len]))
            {
                break;
            }
        }
        if (len <= 0)
        {
            return NULL;
        }
        exe[len] = L'\0';
    }
    return NULL;
}

static char* kt_appdata_root(void)
{
    wchar_t wbuf[KT_MAX_LONG_PATH];
    DWORD n;
    char* path;
    path = kt_project_data_root();
    if (path && path[0])
    {
        return path;
    }
    n = GetEnvironmentVariableW(L"APPDATA", wbuf, KT_MAX_LONG_PATH);
    if (n == 0 || n >= (DWORD)KT_MAX_LONG_PATH)
    {
        return NULL;
    }
    path = kt_utf16_to_utf8(wbuf);
    return path;
}

static int kt_join_into(const char* a, const char* b, char* buf, int cch)
{
    KtBuf k;
    kt_buf_init(&k, buf, cch);
    if (a && a[0])
    {
        kt_buf_add(&k, a);
        if (!kt_sep(a[strlen(a) - 1]))
        {
            kt_buf_addc(&k, '\\');
        }
    }
    if (b)
    {
        while (kt_sep(*b))
        {
            b++;
        }
        kt_buf_add(&k, b);
    }
    return kt_buf_finish(&k);
}

int kt_path_join(const char* a, const char* b, char* buf, int cch)
{
    if (!a || !b || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (b[0] && ((kt_sep(b[0]) && !kt_sep(b[1])) || (b[1] == ':' && kt_sep(b[2]))))
    {
        if ((int)strlen(b) + 1 > cch)
        {
            kt_err("buffer too small");
            return KT_ERR;
        }
        memcpy(buf, b, strlen(b) + 1);
        return KT_OK;
    }
    return kt_join_into(a, b, buf, cch);
}

int kt_path_dirname(const char* path, char* buf, int cch)
{
    int i;
    if (!path || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    i = (int)strlen(path);
    while (i > 0 && !kt_sep(path[i - 1]))
    {
        i--;
    }
    while (i > 1 && kt_sep(path[i - 1]))
    {
        i--;
    }
    if (i == 0)
    {
        if (cch < 2)
        {
            kt_err("buffer too small");
            return KT_ERR;
        }
        buf[0] = '.';
        buf[1] = '\0';
        return KT_OK;
    }
    if (i + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, path, (size_t)i);
    buf[i] = '\0';
    return KT_OK;
}

int kt_path_basename(const char* path, char* buf, int cch)
{
    int i;
    int n;
    if (!path || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    i = (int)strlen(path);
    while (i > 0 && !kt_sep(path[i - 1]))
    {
        i--;
    }
    n = (int)strlen(path + i);
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, path + i, (size_t)n + 1);
    return KT_OK;
}

int kt_path_exists_dir(const char* path)
{
    wchar_t* w;
    DWORD attr;
    if (!path || !path[0])
    {
        return 0;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        return 0;
    }
    attr = GetFileAttributesW(w);
    free(w);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return 0;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

int kt_file_exists(const char* path)
{
    wchar_t* w;
    DWORD attr;
    if (!path || !path[0])
    {
        return 0;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        return 0;
    }
    attr = GetFileAttributesW(w);
    free(w);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return 0;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
}

int kt_dir_create(const char* path)
{
    wchar_t* w;
    wchar_t* p;
    DWORD attr;
    if (!path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    attr = GetFileAttributesW(w);
    if (attr != INVALID_FILE_ATTRIBUTES)
    {
        int ok = (attr & FILE_ATTRIBUTE_DIRECTORY) ? KT_OK : KT_ERR;
        if (ok != KT_OK)
        {
            kt_err("path exists and is not a directory");
        }
        free(w);
        return ok;
    }
    for (p = w + 1; *p; p++)
    {
        if (*p == L'\\' || *p == L'/')
        {
            wchar_t saved = *p;
            *p = L'\0';
            CreateDirectoryW(w, NULL);
            *p = saved;
        }
    }
    if (!CreateDirectoryW(w, NULL))
    {
        DWORD e = GetLastError();
        if (e != ERROR_ALREADY_EXISTS)
        {
            kt_err("create directory failed");
            free(w);
            return KT_ERR;
        }
    }
    free(w);
    return KT_OK;
}

int kt_dir_empty(const char* path)
{
    wchar_t* wdir;
    wchar_t wpattern[KT_MAX_LONG_PATH + 8];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int empty;
    if (!path || !path[0])
    {
        return 0;
    }
    wdir = kt_utf8_to_utf16(path);
    if (!wdir)
    {
        return 0;
    }
    if (swprintf(wpattern, KT_MAX_LONG_PATH + 8, L"%ls\\*", wdir) < 0)
    {
        free(wdir);
        return 0;
    }
    h = FindFirstFileW(wpattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        free(wdir);
        return 0;
    }
    empty = 1;
    while (1)
    {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
        {
            empty = 0;
            break;
        }
        if (!FindNextFileW(h, &fd))
        {
            break;
        }
    }
    FindClose(h);
    free(wdir);
    return empty;
}

int kt_path_is_local(const char* path)
{
    if (!path || !path[0])
    {
        return 0;
    }
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] == ':' &&
        kt_sep(path[2]))
    {
        return 1;
    }
    if (kt_sep(path[0]) && kt_sep(path[1]))
    {
        return 1;
    }
    return 0;
}

int kt_file_write_bytes(const char* path, const void* data, int size)
{
    wchar_t* w;
    HANDLE h;
    DWORD written = 0;
    if (!path || !data || size < 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    h = CreateFileW(w, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(w);
    if (h == INVALID_HANDLE_VALUE)
    {
        kt_err("file create failed");
        return KT_ERR;
    }
    if (size > 0 && !WriteFile(h, data, (DWORD)size, &written, NULL))
    {
        CloseHandle(h);
        kt_err("file write failed");
        return KT_ERR;
    }
    CloseHandle(h);
    if ((int)written != size)
    {
        kt_err("file write incomplete");
        return KT_ERR;
    }
    return KT_OK;
}

int kt_file_write_text(const char* path, const char* text)
{
    if (!text)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    return kt_file_write_bytes(path, text, (int)strlen(text));
}

int kt_file_read_text(const char* path, char* buf, int cch)
{
    char* text;
    int n;
    if (!path || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    text = kt_file_read_utf8(path);
    if (!text)
    {
        return KT_ERR;
    }
    n = (int)strlen(text);
    if (n + 1 > cch)
    {
        free(text);
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, text, (size_t)n + 1);
    free(text);
    return n;
}

int kt_file_delete(const char* path)
{
    wchar_t* w;
    DWORD attr;
    int rc = KT_OK;
    if (!path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    attr = GetFileAttributesW(w);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        if (!RemoveDirectoryW(w))
        {
            kt_err("remove directory failed");
            rc = KT_ERR;
        }
    }
    else if (attr != INVALID_FILE_ATTRIBUTES)
    {
        if (!DeleteFileW(w))
        {
            kt_err("delete file failed");
            rc = KT_ERR;
        }
    }
    free(w);
    return rc;
}

static int kt_ext_match(const char* name, const char* ext)
{
    const char* dot;
    const char* p;
    const char* q;
    if (!ext || !ext[0])
    {
        return 1;
    }
    if (ext[0] == '.')
    {
        ext++;
    }
    dot = strrchr(name, '.');
    if (!dot)
    {
        return 0;
    }
    p = dot + 1;
    q = ext;
    while (*p && *q)
    {
        char a = *p;
        char b = *q;
        if (a >= 'A' && a <= 'Z')
        {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b)
        {
            return 0;
        }
        p++;
        q++;
    }
    return (*p == '\0' && *q == '\0') ? 1 : 0;
}

int kt_dir_list(const char* dir, const char* ext, char* buf, int cch, int* count)
{
    wchar_t* wdir;
    wchar_t wpattern[KT_MAX_LONG_PATH + 8];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    KtBuf k;
    int n = 0;
    int rc = KT_OK;
    int first = 1;
    if (count)
    {
        *count = 0;
    }
    if (!dir || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    wdir = kt_utf8_to_utf16(dir);
    if (!wdir)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    if (swprintf(wpattern, KT_MAX_LONG_PATH + 8, L"%ls\\*", wdir) < 0)
    {
        free(wdir);
        kt_err("path too long");
        return KT_ERR;
    }
    free(wdir);
    h = FindFirstFileW(wpattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        return KT_OK;
    }
    kt_buf_init(&k, buf, cch);
    while (1)
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            char name[KT_MAX_PATHBUF];
            if (kt_utf16_to_utf8_buf(fd.cFileName, (int)wcslen(fd.cFileName), name,
                                     KT_MAX_PATHBUF) == KT_OK)
            {
                if (kt_ext_match(name, ext))
                {
                    size_t len = strlen(dir) + strlen(name) + 2;
                    char* full = (char*)malloc(len);
                    if (full)
                    {
                        sprintf(full, "%s\\%s", dir, name);
                        if (!first && kt_buf_addc(&k, '\n') != KT_OK)
                        {
                            rc = KT_ERR;
                        }
                        if (kt_buf_add(&k, full) != KT_OK)
                        {
                            rc = KT_ERR;
                        }
                        first = 0;
                        n++;
                        free(full);
                    }
                }
            }
        }
        if (!FindNextFileW(h, &fd))
        {
            break;
        }
    }
    FindClose(h);
    if (kt_buf_finish(&k) != KT_OK)
    {
        rc = KT_ERR;
    }
    if (count)
    {
        *count = n;
    }
    return rc;
}

int kt_dir_clear(const char* dir)
{
    wchar_t* wdir;
    wchar_t wpattern[KT_MAX_LONG_PATH + 8];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int rc = KT_OK;
    if (!dir || !dir[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    wdir = kt_utf8_to_utf16(dir);
    if (!wdir)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    if (swprintf(wpattern, KT_MAX_LONG_PATH + 8, L"%ls\\*", wdir) < 0)
    {
        free(wdir);
        kt_err("path too long");
        return KT_ERR;
    }
    h = FindFirstFileW(wpattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        free(wdir);
        return KT_OK;
    }
    while (1)
    {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
        {
            size_t total = wcslen(wdir) + wcslen(fd.cFileName) + 2;
            wchar_t* full = (wchar_t*)malloc(total * sizeof(wchar_t));
            if (full)
            {
                swprintf(full, total, L"%ls\\%ls", wdir, fd.cFileName);
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!RemoveDirectoryW(full))
                    {
                        rc = KT_ERR;
                    }
                }
                else
                {
                    SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
                    if (!DeleteFileW(full))
                    {
                        rc = KT_ERR;
                    }
                }
                free(full);
            }
            else
            {
                rc = KT_ERR;
            }
        }
        if (!FindNextFileW(h, &fd))
        {
            break;
        }
    }
    FindClose(h);
    free(wdir);
    if (rc != KT_OK)
    {
        kt_err("clear directory failed");
    }
    return rc;
}

int kt_appdata_dir(char* buf, int cch)
{
    char* root;
    int rc;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    root = kt_appdata_root();
    if (!root)
    {
        kt_err("APPDATA not found");
        return KT_ERR;
    }
    rc = kt_join_into(root, "koiTile", buf, cch);
    free(root);
    if (rc != KT_OK)
    {
        return rc;
    }
    return kt_dir_create(buf);
}

int kt_tileassets_dir(char* buf, int cch)
{
    char app[KT_MAX_PATHBUF];
    int rc;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_appdata_dir(app, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    rc = kt_join_into(app, "TileAssets", buf, cch);
    if (rc != KT_OK)
    {
        return rc;
    }
    return kt_dir_create(buf);
}

int kt_start_apps_dir(char* buf, int cch)
{
    char* root;
    char dir[KT_MAX_PATHBUF];
    int rc;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    root = kt_appdata_root();
    if (!root)
    {
        kt_err("APPDATA not found");
        return KT_ERR;
    }
    rc = kt_join_into(root, "Microsoft\\Windows\\Start Menu\\Programs", dir, cch > KT_MAX_PATHBUF ? KT_MAX_PATHBUF : cch);
    free(root);
    if (rc != KT_OK)
    {
        return rc;
    }
    rc = kt_join_into(dir, "KoiTile", buf, cch);
    if (rc != KT_OK)
    {
        return rc;
    }
    return kt_dir_create(buf);
}

int kt_config_path(char* buf, int cch)
{
    char app[KT_MAX_PATHBUF];
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_appdata_dir(app, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_join_into(app, "config.json", buf, cch);
}

int kt_temp_dir(char* buf, int cch)
{
    wchar_t wbuf[KT_MAX_LONG_PATH];
    DWORD n;
    char* path;
    int rc;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    n = GetTempPathW(KT_MAX_LONG_PATH, wbuf);
    if (n == 0)
    {
        kt_err("temp path unavailable");
        return KT_ERR;
    }
    path = kt_utf16_to_utf8(wbuf);
    if (!path)
    {
        kt_err("temp path conversion failed");
        return KT_ERR;
    }
    rc = kt_join_into(path, "koiTile", buf, cch);
    free(path);
    if (rc != KT_OK)
    {
        return rc;
    }
    return kt_dir_create(buf);
}

int kt_size_pixels(int size, int* width, int* height)
{
    if (!width || !height)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    switch (size)
    {
    case KT_SIZE_SMALL:
        *width = 70;
        *height = 70;
        return KT_OK;
    case KT_SIZE_MEDIUM:
        *width = 150;
        *height = 150;
        return KT_OK;
    case KT_SIZE_WIDE:
        *width = 310;
        *height = 150;
        return KT_OK;
    case KT_SIZE_LARGE:
        *width = 310;
        *height = 310;
        return KT_OK;
    default:
        kt_err("invalid size");
        return KT_ERR;
    }
}

int kt_size_name(int size, char* buf, int cch)
{
    const char* name;
    int n;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    switch (size)
    {
    case KT_SIZE_SMALL:
        name = "Small";
        break;
    case KT_SIZE_MEDIUM:
        name = "Medium";
        break;
    case KT_SIZE_WIDE:
        name = "Wide";
        break;
    case KT_SIZE_LARGE:
        name = "Large";
        break;
    default:
        kt_err("invalid size");
        return KT_ERR;
    }
    n = (int)strlen(name);
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, name, (size_t)n + 1);
    return KT_OK;
}

int kt_config_save_default(KtConfig* cfg)
{
    char path[KT_MAX_PATHBUF];
    if (kt_config_path(path, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_config_save(cfg, path);
}

int kt_manifest_path(const char* shortcut, char* buf, int cch)
{
    char dir[KT_MAX_PATHBUF];
    if (!shortcut || !shortcut[0])
    {
        kt_err("shortcut is empty");
        return KT_ERR;
    }
    if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_join_into(dir, "VisualElementsManifest.xml", buf, cch);
}

int kt_tile_private_name(const char* path, char* buf, int cch)
{
    char base[KT_MAX_PATHBUF];
    int i;
    if (!path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_path_basename(path, base, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    i = (int)strlen(base);
    if (i > 4 && kt_ends_with(base, ".lnk"))
    {
        base[i - 4] = '\0';
    }
    if ((int)strlen(base) + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, base, strlen(base) + 1);
    return KT_OK;
}

int kt_logical_dir(const char* path, char* buf, int cch)
{
    if (!path || !path[0])
    {
        kt_err("shortcut is empty");
        return KT_ERR;
    }
    return kt_path_dirname(path, buf, cch);
}

int kt_tile_file_target(KtConfig* cfg, KtTile* tile, char* buf, int cch)
{
    char name[KT_MAX_PATHBUF];
    const char* value = NULL;
    KtCfgEntry* e;
    if (!cfg || !tile || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    e = kt_tile_field(tile, "name", 0);
    if (e && e->value)
    {
        value = e->value;
    }
    if (!value || !value[0])
    {
        value = tile->id;
    }
    if (!value || !value[0])
    {
        kt_err("tile name unavailable");
        return KT_ERR;
    }
    if ((int)strlen(value) + 1 > KT_MAX_PATHBUF)
    {
        kt_err("tile name too long");
        return KT_ERR;
    }
    memcpy(name, value, strlen(value) + 1);
    if ((int)strlen(name) + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, name, strlen(name) + 1);
    return KT_OK;
}
