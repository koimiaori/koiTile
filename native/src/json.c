#include "koitile_internal.h"

typedef struct KtJsonParser
{
    const char* p;
    int depth;
} KtJsonParser;

static void kt_json_skip_ws(KtJsonParser* ps);
static KtJson* kt_json_parse_value(KtJsonParser* ps);
static void kt_json_dump_value(KtBuf* b, KtJson* v);
static void kt_json_dump_string(KtBuf* b, const char* s);
static void kt_json_dump_number(KtBuf* b, long long n);

static void kt_json_skip_ws(KtJsonParser* ps)
{
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\r' || *ps->p == '\n')
    {
        ps->p++;
    }
}

KtJson* kt_json_new_object(void)
{
    KtJson* v = (KtJson*)calloc(1, sizeof(KtJson));
    if (v)
    {
        v->type = KT_JSON_OBJECT;
    }
    return v;
}

KtJson* kt_json_new_array(void)
{
    KtJson* v = (KtJson*)calloc(1, sizeof(KtJson));
    if (v)
    {
        v->type = KT_JSON_ARRAY;
    }
    return v;
}

KtJson* kt_json_new_string(const char* s)
{
    KtJson* v = (KtJson*)calloc(1, sizeof(KtJson));
    if (!v)
    {
        return NULL;
    }
    v->type = KT_JSON_STRING;
    v->text = kt_strdup(s ? s : "");
    if (!v->text)
    {
        free(v);
        return NULL;
    }
    return v;
}

KtJson* kt_json_new_number(long long n)
{
    KtJson* v = (KtJson*)calloc(1, sizeof(KtJson));
    if (v)
    {
        v->type = KT_JSON_NUMBER;
        v->number = n;
    }
    return v;
}

KtJson* kt_json_new_bool(int b)
{
    KtJson* v = (KtJson*)calloc(1, sizeof(KtJson));
    if (v)
    {
        v->type = KT_JSON_BOOL;
        v->number = b ? 1 : 0;
    }
    return v;
}

void kt_json_free(KtJson* v)
{
    int i;
    KtJsonMember* m;
    KtJsonMember* next;
    if (!v)
    {
        return;
    }
    free(v->text);
    for (i = 0; i < v->count; i++)
    {
        kt_json_free(v->items[i]);
    }
    free(v->items);
    m = v->members;
    while (m)
    {
        next = m->next;
        free(m->key);
        kt_json_free(m->value);
        free(m);
        m = next;
    }
    free(v);
}

KtJsonMember* kt_json_member(KtJson* obj, const char* key)
{
    KtJsonMember* m;
    if (!obj || obj->type != KT_JSON_OBJECT || !key)
    {
        return NULL;
    }
    m = obj->members;
    while (m)
    {
        if (kt_streq(m->key, key))
        {
            return m;
        }
        m = m->next;
    }
    return NULL;
}

KtJson* kt_json_get(KtJson* obj, const char* key)
{
    KtJsonMember* m = kt_json_member(obj, key);
    return m ? m->value : NULL;
}

const char* kt_json_get_string(KtJson* obj, const char* key, const char* fallback)
{
    KtJson* v = kt_json_get(obj, key);
    if (v && v->type == KT_JSON_STRING && v->text)
    {
        return v->text;
    }
    return fallback;
}

int kt_json_get_int(KtJson* obj, const char* key, int fallback)
{
    KtJson* v = kt_json_get(obj, key);
    if (v && v->type == KT_JSON_NUMBER)
    {
        return (int)v->number;
    }
    if (v && v->type == KT_JSON_BOOL)
    {
        return v->number ? 1 : 0;
    }
    if (v && v->type == KT_JSON_STRING && v->text)
    {
        return atoi(v->text);
    }
    return fallback;
}

void kt_json_set(KtJson* obj, const char* key, KtJson* value)
{
    KtJsonMember* m;
    if (!obj || obj->type != KT_JSON_OBJECT || !key)
    {
        kt_json_free(value);
        return;
    }
    m = kt_json_member(obj, key);
    if (m)
    {
        kt_json_free(m->value);
        m->value = value;
        return;
    }
    m = (KtJsonMember*)calloc(1, sizeof(KtJsonMember));
    if (!m)
    {
        kt_json_free(value);
        return;
    }
    m->key = kt_strdup(key);
    m->value = value;
    if (obj->members_tail)
    {
        obj->members_tail->next = m;
    }
    else
    {
        obj->members = m;
    }
    obj->members_tail = m;
}

void kt_json_set_string(KtJson* obj, const char* key, const char* value)
{
    kt_json_set(obj, key, kt_json_new_string(value ? value : ""));
}

void kt_json_set_int(KtJson* obj, const char* key, int value)
{
    kt_json_set(obj, key, kt_json_new_number((long long)value));
}

void kt_json_add(KtJson* arr, KtJson* value)
{
    KtJson** grown;
    int cap;
    if (!arr || arr->type != KT_JSON_ARRAY)
    {
        kt_json_free(value);
        return;
    }
    if (arr->count >= arr->capacity)
    {
        cap = arr->capacity ? arr->capacity * 2 : 8;
        grown = (KtJson**)realloc(arr->items, (size_t)cap * sizeof(KtJson*));
        if (!grown)
        {
            kt_json_free(value);
            return;
        }
        arr->items = grown;
        arr->capacity = cap;
    }
    arr->items[arr->count] = value;
    arr->count++;
}

static int kt_json_hex4(const char* p, unsigned* out)
{
    unsigned value = 0;
    int i;
    for (i = 0; i < 4; i++)
    {
        char c = p[i];
        value <<= 4;
        if (c >= '0' && c <= '9')
        {
            value |= (unsigned)(c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            value |= (unsigned)(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            value |= (unsigned)(c - 'A' + 10);
        }
        else
        {
            return 0;
        }
    }
    *out = value;
    return 1;
}

static int kt_json_encode_utf8(KtBuf* b, unsigned cp)
{
    if (cp < 0x80u)
    {
        return kt_buf_addc(b, (char)cp);
    }
    if (cp < 0x800u)
    {
        if (kt_buf_addc(b, (char)(0xC0u | (cp >> 6))) != KT_OK)
        {
            return KT_ERR;
        }
        return kt_buf_addc(b, (char)(0x80u | (cp & 0x3Fu)));
    }
    if (cp < 0x10000u)
    {
        if (kt_buf_addc(b, (char)(0xE0u | (cp >> 12))) != KT_OK)
        {
            return KT_ERR;
        }
        if (kt_buf_addc(b, (char)(0x80u | ((cp >> 6) & 0x3Fu))) != KT_OK)
        {
            return KT_ERR;
        }
        return kt_buf_addc(b, (char)(0x80u | (cp & 0x3Fu)));
    }
    if (kt_buf_addc(b, (char)(0xF0u | (cp >> 18))) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_buf_addc(b, (char)(0x80u | ((cp >> 12) & 0x3Fu))) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_buf_addc(b, (char)(0x80u | ((cp >> 6) & 0x3Fu))) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_buf_addc(b, (char)(0x80u | (cp & 0x3Fu)));
}

static char* kt_json_parse_string(KtJsonParser* ps)
{
    KtBuf b;
    unsigned cp;
    unsigned low;
    char c;
    if (*ps->p != '"')
    {
        return NULL;
    }
    ps->p++;
    kt_buf_init(&b, NULL, 0);
    while (1)
    {
        c = *ps->p;
        if (c == '\0')
        {
            free(b.buf);
            return NULL;
        }
        if (c == '"')
        {
            ps->p++;
            break;
        }
        if (c == '\\')
        {
            ps->p++;
            c = *ps->p;
            ps->p++;
            switch (c)
            {
            case '"':
                if (kt_buf_addc(&b, '"') != KT_OK) { free(b.buf); return NULL; }
                break;
            case '\\':
                if (kt_buf_addc(&b, '\\') != KT_OK) { free(b.buf); return NULL; }
                break;
            case '/':
                if (kt_buf_addc(&b, '/') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 'b':
                if (kt_buf_addc(&b, '\b') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 'f':
                if (kt_buf_addc(&b, '\f') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 'n':
                if (kt_buf_addc(&b, '\n') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 'r':
                if (kt_buf_addc(&b, '\r') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 't':
                if (kt_buf_addc(&b, '\t') != KT_OK) { free(b.buf); return NULL; }
                break;
            case 'u':
                if (!kt_json_hex4(ps->p, &cp)) { free(b.buf); return NULL; }
                ps->p += 4;
                if (cp >= 0xD800u && cp <= 0xDBFFu && ps->p[0] == '\\' && ps->p[1] == 'u' &&
                    kt_json_hex4(ps->p + 2, &low) && low >= 0xDC00u && low <= 0xDFFFu)
                {
                    ps->p += 6;
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                }
                if (kt_json_encode_utf8(&b, cp) != KT_OK) { free(b.buf); return NULL; }
                break;
            default:
                free(b.buf);
                return NULL;
            }
            continue;
        }
        if (kt_buf_addc(&b, c) != KT_OK)
        {
            free(b.buf);
            return NULL;
        }
        ps->p++;
    }
    if (kt_buf_addc(&b, '\0') != KT_OK)
    {
        free(b.buf);
        return NULL;
    }
    if (!b.buf)
    {
        return kt_strdup("");
    }
    return b.buf;
}

static int kt_json_parse_number(KtJsonParser* ps, long long* out)
{
    int sign = 1;
    int digits = 0;
    long long value = 0;
    if (*ps->p == '-')
    {
        sign = -1;
        ps->p++;
    }
    if (*ps->p == '0')
    {
        ps->p++;
        digits = 1;
    }
    else
    {
        while (*ps->p >= '0' && *ps->p <= '9')
        {
            value = value * 10 + (long long)(*ps->p - '0');
            if (value > 900000000000000000LL)
            {
                value = 900000000000000000LL;
            }
            ps->p++;
            digits++;
        }
    }
    if (*ps->p == '.')
    {
        ps->p++;
        while (*ps->p >= '0' && *ps->p <= '9')
        {
            ps->p++;
        }
    }
    if (*ps->p == 'e' || *ps->p == 'E')
    {
        ps->p++;
        if (*ps->p == '+' || *ps->p == '-')
        {
            ps->p++;
        }
        while (*ps->p >= '0' && *ps->p <= '9')
        {
            ps->p++;
        }
    }
    if (!digits)
    {
        return 0;
    }
    *out = sign * value;
    return 1;
}

static KtJson* kt_json_parse_value(KtJsonParser* ps)
{
    KtJson* v = NULL;
    KtJson* item = NULL;
    KtJson* key = NULL;
    long long number = 0;
    if (ps->depth > 64)
    {
        return NULL;
    }
    kt_json_skip_ws(ps);
    switch (*ps->p)
    {
    case '{':
        ps->p++;
        ps->depth++;
        v = kt_json_new_object();
        if (!v)
        {
            ps->depth--;
            return NULL;
        }
        kt_json_skip_ws(ps);
        if (*ps->p == '}')
        {
            ps->p++;
            ps->depth--;
            return v;
        }
        while (1)
        {
            kt_json_skip_ws(ps);
            key = NULL;
            if (*ps->p == '"')
            {
                char* name = kt_json_parse_string(ps);
                if (!name)
                {
                    kt_json_free(v);
                    ps->depth--;
                    return NULL;
                }
                key = kt_json_new_string(name);
                free(name);
            }
            if (!key)
            {
                kt_json_free(v);
                ps->depth--;
                return NULL;
            }
            kt_json_skip_ws(ps);
            if (*ps->p != ':')
            {
                kt_json_free(key);
                kt_json_free(v);
                ps->depth--;
                return NULL;
            }
            ps->p++;
            item = kt_json_parse_value(ps);
            if (!item)
            {
                kt_json_free(key);
                kt_json_free(v);
                ps->depth--;
                return NULL;
            }
            kt_json_set(v, key->text, item);
            kt_json_free(key);
            kt_json_skip_ws(ps);
            if (*ps->p == ',')
            {
                ps->p++;
                continue;
            }
            if (*ps->p == '}')
            {
                ps->p++;
                break;
            }
            kt_json_free(v);
            ps->depth--;
            return NULL;
        }
        ps->depth--;
        return v;
    case '[':
        ps->p++;
        ps->depth++;
        v = kt_json_new_array();
        if (!v)
        {
            ps->depth--;
            return NULL;
        }
        kt_json_skip_ws(ps);
        if (*ps->p == ']')
        {
            ps->p++;
            ps->depth--;
            return v;
        }
        while (1)
        {
            item = kt_json_parse_value(ps);
            if (!item)
            {
                kt_json_free(v);
                ps->depth--;
                return NULL;
            }
            kt_json_add(v, item);
            kt_json_skip_ws(ps);
            if (*ps->p == ',')
            {
                ps->p++;
                continue;
            }
            if (*ps->p == ']')
            {
                ps->p++;
                break;
            }
            kt_json_free(v);
            ps->depth--;
            return NULL;
        }
        ps->depth--;
        return v;
    case '"':
    {
        char* s = kt_json_parse_string(ps);
        if (!s)
        {
            return NULL;
        }
        v = kt_json_new_string(s);
        free(s);
        return v;
    }
    case 't':
        if (strncmp(ps->p, "true", 4) == 0)
        {
            ps->p += 4;
            return kt_json_new_bool(1);
        }
        return NULL;
    case 'f':
        if (strncmp(ps->p, "false", 5) == 0)
        {
            ps->p += 5;
            return kt_json_new_bool(0);
        }
        return NULL;
    case 'n':
        if (strncmp(ps->p, "null", 4) == 0)
        {
            ps->p += 4;
            v = (KtJson*)calloc(1, sizeof(KtJson));
            if (v)
            {
                v->type = KT_JSON_NULL;
            }
            return v;
        }
        return NULL;
    default:
        if (kt_json_parse_number(ps, &number))
        {
            return kt_json_new_number(number);
        }
        return NULL;
    }
}

KtJson* kt_json_parse(const char* text)
{
    KtJsonParser ps;
    KtJson* v;
    if (!text)
    {
        return NULL;
    }
    ps.p = text;
    ps.depth = 0;
    v = kt_json_parse_value(&ps);
    if (!v)
    {
        return NULL;
    }
    kt_json_skip_ws(&ps);
    if (*ps.p != '\0')
    {
        kt_json_free(v);
        return NULL;
    }
    return v;
}

KtJson* kt_json_parse_file(const char* path)
{
    char* text = kt_file_read_utf8(path);
    KtJson* v;
    if (!text)
    {
        return NULL;
    }
    v = kt_json_parse(text);
    free(text);
    return v;
}

int kt_json_utf8_dec(const char* s, unsigned* cp)
{
    unsigned char c = (unsigned char)s[0];
    unsigned char c1;
    unsigned char c2;
    unsigned char c3;
    if (c < 0x80u)
    {
        *cp = c;
        return 1;
    }
    if (c >= 0xC0u && c < 0xE0u)
    {
        c1 = (unsigned char)s[1];
        if ((c1 & 0xC0u) != 0x80u)
        {
            *cp = 0xFFFDu;
            return 1;
        }
        *cp = ((unsigned)(c & 0x1Fu) << 6) | (unsigned)(c1 & 0x3Fu);
        return 2;
    }
    if (c >= 0xE0u && c < 0xF0u)
    {
        c1 = (unsigned char)s[1];
        c2 = (unsigned char)s[2];
        if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u)
        {
            *cp = 0xFFFDu;
            return 1;
        }
        *cp = ((unsigned)(c & 0x0Fu) << 12) | ((unsigned)(c1 & 0x3Fu) << 6) | (unsigned)(c2 & 0x3Fu);
        return 3;
    }
    if (c >= 0xF0u)
    {
        c1 = (unsigned char)s[1];
        c2 = (unsigned char)s[2];
        c3 = (unsigned char)s[3];
        if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u || (c3 & 0xC0u) != 0x80u)
        {
            *cp = 0xFFFDu;
            return 1;
        }
        *cp = ((unsigned)(c & 0x07u) << 18) | ((unsigned)(c1 & 0x3Fu) << 12) |
              ((unsigned)(c2 & 0x3Fu) << 6) | (unsigned)(c3 & 0x3Fu);
        return 4;
    }
    *cp = 0xFFFDu;
    return 1;
}

static void kt_json_dump_string(KtBuf* b, const char* s)
{
    unsigned cp;
    int n;
    char esc[8];
    if (!s)
    {
        s = "";
    }
    kt_buf_addc(b, '"');
    while (*s)
    {
        unsigned char c = (unsigned char)*s;
        if (c == '"')
        {
            kt_buf_addn(b, "\\\"", 2);
            s++;
            continue;
        }
        if (c == '\\')
        {
            kt_buf_addn(b, "\\\\", 2);
            s++;
            continue;
        }
        if (c < 0x20u)
        {
            switch (c)
            {
            case '\b':
                kt_buf_addn(b, "\\b", 2);
                break;
            case '\f':
                kt_buf_addn(b, "\\f", 2);
                break;
            case '\n':
                kt_buf_addn(b, "\\n", 2);
                break;
            case '\r':
                kt_buf_addn(b, "\\r", 2);
                break;
            case '\t':
                kt_buf_addn(b, "\\t", 2);
                break;
            default:
                esc[0] = '\\';
                esc[1] = 'u';
                esc[2] = '0';
                esc[3] = '0';
                esc[4] = (char)("0123456789abcdef"[(c >> 4) & 0xF]);
                esc[5] = (char)("0123456789abcdef"[c & 0xF]);
                esc[6] = '\0';
                kt_buf_addn(b, esc, 6);
                break;
            }
            s++;
            continue;
        }
        n = kt_json_utf8_dec(s, &cp);
        kt_buf_addn(b, s, n);
        s += n;
    }
    kt_buf_addc(b, '"');
}

static void kt_json_dump_number(KtBuf* b, long long n)
{
    char tmp[32];
    int i = 0;
    int j;
    char out[32];
    unsigned long long mag;
    int neg = 0;
    if (n < 0)
    {
        neg = 1;
        mag = (unsigned long long)(-(n + 1)) + 1ull;
    }
    else
    {
        mag = (unsigned long long)n;
    }
    if (mag == 0)
    {
        kt_buf_addc(b, '0');
        return;
    }
    while (mag > 0)
    {
        tmp[i] = (char)('0' + (int)(mag % 10ull));
        mag /= 10ull;
        i++;
    }
    j = 0;
    if (neg)
    {
        out[j] = '-';
        j++;
    }
    while (i > 0)
    {
        i--;
        out[j] = tmp[i];
        j++;
    }
    kt_buf_addn(b, out, j);
}

static void kt_json_dump_value(KtBuf* b, KtJson* v)
{
    int i;
    KtJsonMember* m;
    if (!v)
    {
        kt_buf_addn(b, "null", 4);
        return;
    }
    switch (v->type)
    {
    case KT_JSON_NULL:
        kt_buf_addn(b, "null", 4);
        break;
    case KT_JSON_BOOL:
        if (v->number)
        {
            kt_buf_addn(b, "true", 4);
        }
        else
        {
            kt_buf_addn(b, "false", 5);
        }
        break;
    case KT_JSON_NUMBER:
        kt_json_dump_number(b, v->number);
        break;
    case KT_JSON_STRING:
        kt_json_dump_string(b, v->text);
        break;
    case KT_JSON_ARRAY:
        kt_buf_addc(b, '[');
        for (i = 0; i < v->count; i++)
        {
            if (i)
            {
                kt_buf_addc(b, ',');
            }
            kt_json_dump_value(b, v->items[i]);
        }
        kt_buf_addc(b, ']');
        break;
    case KT_JSON_OBJECT:
        kt_buf_addc(b, '{');
        m = v->members;
        while (m)
        {
            if (m != v->members)
            {
                kt_buf_addc(b, ',');
            }
            kt_json_dump_string(b, m->key);
            kt_buf_addc(b, ':');
            kt_json_dump_value(b, m->value);
            m = m->next;
        }
        kt_buf_addc(b, '}');
        break;
    default:
        kt_buf_addn(b, "null", 4);
        break;
    }
}

char* kt_json_dump(KtJson* v)
{
    KtBuf b;
    if (!v)
    {
        return NULL;
    }
    kt_buf_init(&b, NULL, 0);
    kt_json_dump_value(&b, v);
    if (b.len < 0)
    {
        return NULL;
    }
    kt_buf_addc(&b, '\0');
    if (!b.buf)
    {
        return kt_strdup("");
    }
    return b.buf;
}
