#include "koitile_internal.h"

static char kt_error_text[512] = {0};

void kt_err(const char* message)
{
    int i = 0;
    if (!message)
    {
        kt_error_text[0] = '\0';
        return;
    }
    while (message[i] && i < (int)sizeof(kt_error_text) - 1)
    {
        kt_error_text[i] = message[i];
        i++;
    }
    kt_error_text[i] = '\0';
}

const char* kt_error_text_copy(void)
{
    return kt_error_text;
}

char* kt_strdup(const char* s)
{
    size_t n;
    char* p;
    if (!s)
    {
        return NULL;
    }
    n = strlen(s) + 1;
    p = (char*)malloc(n);
    if (!p)
    {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

char* kt_strndup(const char* s, int n)
{
    char* p;
    if (!s || n < 0)
    {
        return NULL;
    }
    p = (char*)malloc((size_t)n + 1);
    if (!p)
    {
        return NULL;
    }
    if (n > 0)
    {
        memcpy(p, s, (size_t)n);
    }
    p[n] = '\0';
    return p;
}

int kt_streq(const char* a, const char* b)
{
    if (a == b)
    {
        return 1;
    }
    if (!a || !b)
    {
        return 0;
    }
    return strcmp(a, b) == 0;
}

int kt_starts_with(const char* s, const char* prefix)
{
    size_t n;
    if (!s || !prefix)
    {
        return 0;
    }
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

int kt_ends_with(const char* s, const char* suffix)
{
    size_t a;
    size_t b;
    if (!s || !suffix)
    {
        return 0;
    }
    a = strlen(s);
    b = strlen(suffix);
    if (b > a)
    {
        return 0;
    }
    return strcmp(s + (a - b), suffix) == 0;
}

char* kt_trim(char* s)
{
    char* end;
    if (!s)
    {
        return NULL;
    }
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    {
        s++;
    }
    end = s + strlen(s);
    while (end > s)
    {
        char c = end[-1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
        {
            break;
        }
        end--;
    }
    *end = '\0';
    return s;
}

int kt_append(char* buf, int cch, const char* s)
{
    int used;
    int need;
    if (!buf || cch <= 0 || !s)
    {
        return KT_ERR;
    }
    used = (int)strlen(buf);
    need = (int)strlen(s);
    if (used + need + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf + used, s, (size_t)need + 1);
    return KT_OK;
}

void kt_buf_init(KtBuf* b, char* buf, int cch)
{
    b->buf = buf;
    b->cch = cch;
    b->len = 0;
    b->growable = (buf && cch > 0) ? 0 : 1;
    if (buf && cch > 0)
    {
        buf[0] = '\0';
    }
}

static int kt_buf_grow(KtBuf* b, int extra)
{
    int need;
    int cap;
    char* grown;
    if (b->len < 0)
    {
        return KT_ERR;
    }
    if (extra < 0 || b->len > 0x40000000 - extra)
    {
        b->len = -1;
        kt_err("buffer too large");
        return KT_ERR;
    }
    need = b->len + extra + 1;
    if (need <= b->cch)
    {
        return KT_OK;
    }
    if (!b->growable)
    {
        b->len = -1;
        kt_err("buffer too small");
        return KT_ERR;
    }
    cap = b->cch > 64 ? b->cch : 64;
    while (cap < need)
    {
        if (cap > 0x40000000)
        {
            b->len = -1;
            kt_err("buffer too large");
            return KT_ERR;
        }
        cap *= 2;
    }
    grown = (char*)realloc(b->buf, (size_t)cap);
    if (!grown)
    {
        b->len = -1;
        kt_err("out of memory");
        return KT_ERR;
    }
    b->buf = grown;
    b->cch = cap;
    return KT_OK;
}

int kt_buf_addn(KtBuf* b, const char* s, int n)
{
    if (!b || !s || n < 0)
    {
        return KT_ERR;
    }
    if (kt_buf_grow(b, n) != KT_OK)
    {
        return KT_ERR;
    }
    if (n > 0)
    {
        memcpy(b->buf + b->len, s, (size_t)n);
        b->len += n;
    }
    b->buf[b->len] = '\0';
    return KT_OK;
}

int kt_buf_add(KtBuf* b, const char* s)
{
    if (!s)
    {
        return KT_OK;
    }
    return kt_buf_addn(b, s, (int)strlen(s));
}

int kt_buf_addc(KtBuf* b, char c)
{
    return kt_buf_addn(b, &c, 1);
}

int kt_buf_result(KtBuf* b)
{
    if (!b || b->len < 0)
    {
        return KT_ERR;
    }
    return KT_OK;
}

int kt_buf_finish(KtBuf* b)
{
    if (!b || b->len < 0)
    {
        return KT_ERR;
    }
    if (b->buf)
    {
        b->buf[b->len] = '\0';
    }
    return KT_OK;
}

wchar_t* kt_utf8_to_utf16(const char* s)
{
    int n;
    wchar_t* w;
    if (!s)
    {
        return NULL;
    }
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0)
    {
        return NULL;
    }
    w = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (!w)
    {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0)
    {
        free(w);
        return NULL;
    }
    return w;
}

char* kt_utf16_to_utf8_len(const wchar_t* w, int wlen)
{
    int n;
    char* s;
    if (!w || wlen < 0)
    {
        return NULL;
    }
    if (wlen == 0)
    {
        return kt_strdup("");
    }
    n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        return NULL;
    }
    s = (char*)malloc((size_t)n + 1);
    if (!s)
    {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, wlen, s, n, NULL, NULL) <= 0)
    {
        free(s);
        return NULL;
    }
    s[n] = '\0';
    return s;
}

char* kt_utf16_to_utf8(const wchar_t* w)
{
    if (!w)
    {
        return NULL;
    }
    return kt_utf16_to_utf8_len(w, (int)wcslen(w));
}

int kt_utf16_to_utf8_buf(const wchar_t* w, int wlen, char* buf, int cch)
{
    int n;
    if (!w || !buf || cch <= 0 || wlen < 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    if (wlen == 0)
    {
        return KT_OK;
    }
    n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (n <= 0)
    {
        kt_err("text conversion failed");
        return KT_ERR;
    }
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, wlen, buf, n, NULL, NULL) <= 0)
    {
        buf[0] = '\0';
        kt_err("text conversion failed");
        return KT_ERR;
    }
    buf[n] = '\0';
    return KT_OK;
}

int kt_utf8_init_len(const char* s)
{
    unsigned cp;
    int n;
    int total = 0;
    if (!s)
    {
        return 0;
    }
    while (*s)
    {
        n = kt_json_utf8_dec(s, &cp);
        s += n;
        total += n;
    }
    return total;
}

int kt_clamp_int(int v, int lo, int hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

char* kt_file_read_utf8(const char* path)
{
    wchar_t* wpath;
    HANDLE h;
    LARGE_INTEGER size;
    DWORD got = 0;
    char* raw;
    char* out;
    DWORD i;
    if (!path)
    {
        kt_err("invalid argument");
        return NULL;
    }
    wpath = kt_utf8_to_utf16(path);
    if (!wpath)
    {
        kt_err("invalid path");
        return NULL;
    }
    h = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    free(wpath);
    if (h == INVALID_HANDLE_VALUE)
    {
        kt_err("file open failed");
        return NULL;
    }
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024)
    {
        CloseHandle(h);
        kt_err("file too large");
        return NULL;
    }
    raw = (char*)malloc((size_t)size.QuadPart + 2);
    if (!raw)
    {
        CloseHandle(h);
        kt_err("out of memory");
        return NULL;
    }
    if (!ReadFile(h, raw, (DWORD)size.QuadPart, &got, NULL))
    {
        CloseHandle(h);
        free(raw);
        kt_err("file read failed");
        return NULL;
    }
    CloseHandle(h);
    raw[got] = '\0';
    raw[got + 1] = '\0';
    if (got >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF)
    {
        memmove(raw, raw + 3, (size_t)got - 2);
    }
    else if (got >= 2 && ((unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE))
    {
        out = kt_utf16_to_utf8_len((const wchar_t*)(const void*)(raw + 2), (int)((got - 2) / 2));
        free(raw);
        return out;
    }
    else if (got >= 2 && ((unsigned char)raw[0] == 0xFE && (unsigned char)raw[1] == 0xFF))
    {
        wchar_t* tmp = (wchar_t*)malloc(((size_t)got / 2 + 1) * sizeof(wchar_t));
        if (!tmp)
        {
            free(raw);
            kt_err("out of memory");
            return NULL;
        }
        for (i = 0; i + 1 < got; i += 2)
        {
            tmp[i / 2] = (wchar_t)(((unsigned char)raw[i] << 8) | (unsigned char)raw[i + 1]);
        }
        tmp[i / 2] = L'\0';
        out = kt_utf16_to_utf8(tmp);
        free(tmp);
        free(raw);
        return out;
    }
    return raw;
}

int kt_file_list_add(char*** list, int* count, const char* s)
{
    char** grown;
    char* copy;
    if (!list || !count || !s)
    {
        return KT_ERR;
    }
    grown = (char**)realloc(*list, ((size_t)*count + 1) * sizeof(char*));
    if (!grown)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    *list = grown;
    copy = kt_strdup(s);
    if (!copy)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    grown[*count] = copy;
    *count = *count + 1;
    return KT_OK;
}

void kt_file_list_free(char** list, int count)
{
    int i;
    if (!list)
    {
        return;
    }
    for (i = 0; i < count; i++)
    {
        free(list[i]);
    }
    free(list);
}
