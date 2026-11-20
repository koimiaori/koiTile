#include "koitile_internal.h"

static int kt_text_source_value(KtConfig* cfg, const char* id, char* buf, int cch)
{
    if (kt_config_get(cfg, id, "textSourcePath", buf, cch) != KT_OK)
    {
        buf[0] = '\0';
    }
    return KT_OK;
}

int kt_text_apply_template(const char* tmpl, const char* content, char* buf, int cch)
{
    const char* marker = "{content}";
    const char* p;
    KtBuf b;
    if (!buf || cch <= 0 || !content)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    if (!tmpl)
    {
        tmpl = marker;
    }
    kt_buf_init(&b, buf, cch);
    p = tmpl;
    while (*p)
    {
        if (strncmp(p, marker, 9) == 0)
        {
            if (kt_buf_add(&b, content) != KT_OK)
            {
                return KT_ERR;
            }
            p += 9;
            continue;
        }
        if (kt_buf_addc(&b, *p) != KT_OK)
        {
            return KT_ERR;
        }
        p++;
    }
    return kt_buf_finish(&b);
}

int kt_text_fetch(KtConfig* cfg, const char* id, char* buf, int cch)
{
    char raw[KT_MAX_TEXT];
    char source_path[KT_MAX_PATHBUF];
    char template_text[KT_MAX_TEXT];
    int source;
    if (!cfg || !id || !id[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    raw[0] = '\0';
    source_path[0] = '\0';
    if (!kt_config_has_tile(cfg, id))
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    source = kt_config_get_int(cfg, id, "textSource", KT_TEXT_STATIC);
    kt_text_source_value(cfg, id, source_path, KT_MAX_PATHBUF);
    if (source == KT_TEXT_STATIC)
    {
        if ((int)strlen(source_path) + 1 > KT_MAX_TEXT)
        {
            kt_err("static text too long");
            return KT_ERR;
        }
        memcpy(raw, source_path, strlen(source_path) + 1);
    }
    else if (source == KT_TEXT_FILE)
    {
        if (!source_path[0])
        {
            kt_err("text source path is empty");
            return KT_ERR;
        }
        if (kt_file_read_text(source_path, raw, KT_MAX_TEXT) < 0)
        {
            return KT_ERR;
        }
    }
    else if (source == KT_TEXT_URL)
    {
        if (!source_path[0])
        {
            kt_err("text source url is empty");
            return KT_ERR;
        }
        if (kt_http_get(source_path, raw, KT_MAX_TEXT, 15000) != KT_OK)
        {
            return KT_ERR;
        }
    }
    else
    {
        kt_err("invalid text source");
        return KT_ERR;
    }
    template_text[0] = '\0';
    if (kt_config_get(cfg, id, "textTemplate", template_text, KT_MAX_TEXT) != KT_OK ||
        !template_text[0])
    {
        memcpy(template_text, "{content}", 10);
    }
    return kt_text_apply_template(template_text, raw, buf, cch);
}
