#include "koitile_core.h"

int kt_image_info(const char* path, int* width, int* height)
{
    (void)path;
    (void)width;
    (void)height;
    return KT_ERR;
}

int kt_image_resize(const char* src, int dst_w, int dst_h, const char* dst)
{
    (void)src;
    (void)dst_w;
    (void)dst_h;
    (void)dst;
    return KT_ERR;
}

int kt_image_cut_grid(const char* src, int rows, int cols, int cell_w, int cell_h,
                      const char* target_name, const char* out_dir, int* out_count)
{
    (void)src;
    (void)rows;
    (void)cols;
    (void)cell_w;
    (void)cell_h;
    (void)target_name;
    (void)out_dir;
    if (out_count)
    {
        *out_count = 0;
    }
    return KT_ERR;
}

int kt_image_render_tile(KtConfig* cfg, const char* id, int size, const char* dst)
{
    (void)cfg;
    (void)id;
    (void)size;
    (void)dst;
    return KT_ERR;
}

int kt_image_extract_icon(const char* src, int index, int icon_size, const char* dst)
{
    (void)src;
    (void)index;
    (void)icon_size;
    (void)dst;
    return KT_ERR;
}

int kt_image_composite(const char* background, const char* icon, int icon_size, int offset_x,
                       int offset_y, int dst_w, int dst_h, const char* dst)
{
    (void)background;
    (void)icon;
    (void)icon_size;
    (void)offset_x;
    (void)offset_y;
    (void)dst_w;
    (void)dst_h;
    (void)dst;
    return KT_ERR;
}
