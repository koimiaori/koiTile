#include "koitile_internal.h"

static int kt_xml_logo_name(const char* prefix, const char* target, int size, char* buf, int cch)
{
    int width;
    int height;
    int n;
    if (kt_size_pixels(size, &width, &height) != KT_OK)
    {
        kt_err("invalid size");
        return KT_ERR;
    }
    n = sprintf(buf, "%s%s_%dx%d.png", prefix ? prefix : "", target, width, height);
    if (n < 0 || n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    return KT_OK;
}

int kt_xml_build_prefixed(KtConfig* cfg, const char* id, const char* prefix, char* buf, int cch)
{
    char target[KT_MAX_PATHBUF];
    char logo_small[KT_MAX_PATHBUF];
    char logo_medium[KT_MAX_PATHBUF];
    char logo_wide[KT_MAX_PATHBUF];
    char logo_large[KT_MAX_PATHBUF];
    char color[64];
    const char* shown;
    const char* foreground;
    const char* background;
    KtTile* tile;
    KtCfgEntry* e;
    KtBuf b;
    if (!cfg || !id || !id[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_tile_file_target(cfg, tile, target, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_xml_logo_name(prefix, target, KT_SIZE_SMALL, logo_small, KT_MAX_PATHBUF) != KT_OK ||
        kt_xml_logo_name(prefix, target, KT_SIZE_MEDIUM, logo_medium, KT_MAX_PATHBUF) != KT_OK ||
        kt_xml_logo_name(prefix, target, KT_SIZE_WIDE, logo_wide, KT_MAX_PATHBUF) != KT_OK ||
        kt_xml_logo_name(prefix, target, KT_SIZE_LARGE, logo_large, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    shown = kt_config_get_int(cfg, id, "showName", 1) ? "on" : "off";
    foreground = kt_config_get_int(cfg, id, "foregroundLight", 1) ? "light" : "dark";
    background = "#2D89EF";
    if (kt_config_get_int(cfg, id, "colorEnabled", 0))
    {
        e = kt_tile_field(tile, "color", 0);
        if (e && e->value && e->value[0])
        {
            if (strlen(e->value) + 1 > sizeof(color))
            {
                kt_err("color too long");
                return KT_ERR;
            }
            memcpy(color, e->value, strlen(e->value) + 1);
            background = color;
        }
    }
    kt_buf_init(&b, buf, cch);
    kt_buf_add(&b, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    kt_buf_add(&b, "<Application xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n");
    kt_buf_add(&b, "  <VisualElements\n");
    kt_buf_add(&b, "    ShowNameOnSquare150x150Logo=\"");
    kt_buf_add(&b, shown);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    Square70x70Logo=\"");
    kt_buf_add(&b, logo_small);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    Square150x150Logo=\"");
    kt_buf_add(&b, logo_medium);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    Wide310x150Logo=\"");
    kt_buf_add(&b, logo_wide);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    Square310x310Logo=\"");
    kt_buf_add(&b, logo_large);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    ForegroundText=\"");
    kt_buf_add(&b, foreground);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    BackgroundColor=\"");
    kt_buf_add(&b, background);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    ShowNameOnSquare310x310Logo=\"");
    kt_buf_add(&b, shown);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "    ShowNameOnWide310x150Logo=\"");
    kt_buf_add(&b, shown);
    kt_buf_add(&b, "\"\n");
    kt_buf_add(&b, "  />\n");
    kt_buf_add(&b, "</Application>\n");
    if (kt_buf_finish(&b) != KT_OK)
    {
        buf[0] = '\0';
        return KT_ERR;
    }
    return KT_OK;
}

int kt_xml_build_tile(KtConfig* cfg, const char* id, char* buf, int cch)
{
    return kt_xml_build_prefixed(cfg, id, "", buf, cch);
}

int kt_xml_build(KtConfig* cfg, const char* id, char* buf, int cch)
{
    return kt_xml_build_tile(cfg, id, buf, cch);
}
