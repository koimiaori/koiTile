#ifndef KOITILE_CORE_H
#define KOITILE_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define KT_API __declspec(dllexport)
#else
#define KT_API
#endif

#define KT_OK 0
#define KT_ERR (-1)
#define KT_MAX_QUEUE 5
#define KT_MAX_PEEK 32

#define KT_SIZE_SMALL 0
#define KT_SIZE_MEDIUM 1
#define KT_SIZE_WIDE 2
#define KT_SIZE_LARGE 3

#define KT_TEXT_STATIC 0
#define KT_TEXT_FILE 1
#define KT_TEXT_URL 2

typedef struct KtConfig KtConfig;

KT_API int kt_init(void);
KT_API void kt_shutdown(void);
KT_API void kt_free(void* p);
KT_API int kt_last_error(char* buf, int cch);
KT_API int kt_version(char* buf, int cch);

KT_API int kt_appdata_dir(char* buf, int cch);
KT_API int kt_config_path(char* buf, int cch);
KT_API int kt_tileassets_dir(char* buf, int cch);
KT_API int kt_temp_dir(char* buf, int cch);
KT_API int kt_size_pixels(int size, int* width, int* height);
KT_API int kt_size_name(int size, char* buf, int cch);

KT_API KtConfig* kt_config_new(void);
KT_API KtConfig* kt_config_load(const char* path);
KT_API int kt_config_save(KtConfig* cfg, const char* path);
KT_API int kt_config_save_default(KtConfig* cfg);
KT_API void kt_config_free(KtConfig* cfg);
KT_API int kt_config_clear(KtConfig* cfg);
KT_API int kt_config_tile_count(KtConfig* cfg);
KT_API int kt_config_tile_id_at(KtConfig* cfg, int index, char* buf, int cch);
KT_API int kt_config_add_tile(KtConfig* cfg, const char* id);
KT_API int kt_config_remove_tile(KtConfig* cfg, const char* id);
KT_API int kt_config_has_tile(KtConfig* cfg, const char* id);
KT_API int kt_config_rename_tile(KtConfig* cfg, const char* id, const char* new_id);
KT_API int kt_config_get(KtConfig* cfg, const char* id, const char* key, char* buf, int cch);
KT_API int kt_config_set_board(KtConfig* cfg, const char* key, const char* value);
KT_API int kt_config_get_board(KtConfig* cfg, const char* key, char* buf, int cch);
KT_API int kt_config_get_int_board(KtConfig* cfg, const char* key, int fallback);
KT_API int kt_config_set_int_board(KtConfig* cfg, const char* key, int value);
KT_API int kt_config_set(KtConfig* cfg, const char* id, const char* key, const char* value);
KT_API int kt_config_get_int(KtConfig* cfg, const char* id, const char* key, int fallback);
KT_API int kt_config_set_int(KtConfig* cfg, const char* id, const char* key, int value);
KT_API int kt_config_list_count(KtConfig* cfg, const char* id, const char* key);
KT_API int kt_config_list_get(KtConfig* cfg, const char* id, const char* key, int index, char* buf, int cch);
KT_API int kt_config_list_add(KtConfig* cfg, const char* id, const char* key, const char* value);
KT_API int kt_config_list_insert(KtConfig* cfg, const char* id, const char* key, int index, const char* value);
KT_API int kt_config_list_set(KtConfig* cfg, const char* id, const char* key, int index, const char* value);
KT_API int kt_config_list_remove(KtConfig* cfg, const char* id, const char* key, int index);
KT_API int kt_config_list_move(KtConfig* cfg, const char* id, const char* key, int from, int to);
KT_API int kt_config_list_clear(KtConfig* cfg, const char* id, const char* key);
KT_API int kt_config_export(KtConfig* cfg, const char* id, const char* path);
KT_API int kt_config_export_all(KtConfig* cfg, const char* path);
KT_API int kt_config_import(KtConfig* cfg, const char* path, char* id_buf, int cch);
KT_API int kt_config_copy_tile(KtConfig* cfg, const char* src_id, const char* dst_id);

KT_API int kt_tile_config_path(const char* shortcut, char* buf, int cch);
KT_API int kt_tile_config_save_tile(KtConfig* cfg, const char* id);
KT_API int kt_tile_config_load_tile(KtConfig* cfg, const char* id);

KT_API int kt_file_read_text(const char* path, char* buf, int cch);
KT_API int kt_file_write_text(const char* path, const char* text);
KT_API int kt_file_write_bytes(const char* path, const void* data, int size);
KT_API int kt_file_exists(const char* path);
KT_API int kt_file_delete(const char* path);
KT_API int kt_dir_create(const char* path);
KT_API int kt_dir_list(const char* dir, const char* ext, char* buf, int cch, int* count);
KT_API int kt_dir_clear(const char* dir);
KT_API int kt_path_join(const char* a, const char* b, char* buf, int cch);
KT_API int kt_path_dirname(const char* path, char* buf, int cch);
KT_API int kt_path_basename(const char* path, char* buf, int cch);
KT_API int kt_path_exists_dir(const char* path);

KT_API int kt_start_apps_dir(char* buf, int cch);

KT_API int kt_http_get(const char* url, char* buf, int cch, int timeout_ms);

KT_API int kt_xml_build(KtConfig* cfg, const char* id, char* buf, int cch);
KT_API int kt_xml_write(KtConfig* cfg, const char* id);
KT_API int kt_xml_remove(KtConfig* cfg, const char* id);
KT_API int kt_tile_apply(KtConfig* cfg, const char* id);
KT_API int kt_tile_restore(KtConfig* cfg, const char* id);
KT_API int kt_tile_restore_all(KtConfig* cfg);
KT_API int kt_tile_install_start(KtConfig* cfg, const char* id);
KT_API int kt_tile_state(KtConfig* cfg, const char* id);

KT_API int kt_image_info(const char* path, int* width, int* height);
KT_API int kt_image_resize(const char* src, int dst_w, int dst_h, const char* dst);
KT_API int kt_image_cut_grid(const char* src, int rows, int cols, int cell_w, int cell_h, const char* target_name, const char* out_dir, int* out_count);
KT_API int kt_image_render_tile(KtConfig* cfg, const char* id, int size, const char* dst);
KT_API int kt_image_render_fill(int w, int h, const char* color, const char* dst);
KT_API int kt_image_render_clear(int w, int h, const char* dst);
KT_API int kt_image_add_label(const char* src, const char* label, int font_px, int margin, int color, const char* dst);
KT_API int kt_image_add_label_box(const char* src, const char* line1, const char* line2, int font_px, const char* dst);
KT_API int kt_image_render_slice(const char* src, int canvas_w, int canvas_h, int image_w, int image_h, int offset_x, int offset_y, const char* dst);
KT_API int kt_image_extract_icon(const char* src, int index, int icon_size, const char* dst);
KT_API int kt_image_composite(const char* background, const char* icon, int icon_size, int offset_x, int offset_y, int dst_w, int dst_h, const char* dst);

KT_API int kt_shortcut_create(const char* target, const char* link, const char* aumid, const char* args, const char* work_dir);
KT_API int kt_shortcut_aumid(const char* link, char* buf, int cch);
KT_API int kt_shortcut_target_of(const char* link, char* buf, int cch);
KT_API int kt_shell_refresh(void);
KT_API int kt_shell_notify(const char* path);

KT_API int kt_env_explorerpatcher(void);
KT_API int kt_env_win10_start(void);
KT_API int kt_env_tile_notifications(void);
KT_API int kt_env_probe(char* buf, int cch);

KT_API int kt_schedule_reset(void);
KT_API int kt_schedule_sync(KtConfig* cfg);
KT_API int kt_schedule_due(KtConfig* cfg, char* ids, int cch, int* count);
KT_API int kt_schedule_mark(KtConfig* cfg, const char* id);
KT_API int kt_schedule_seconds_left(KtConfig* cfg, const char* id);
KT_API int kt_schedule_due_peek(KtConfig* cfg, char* ids, int cch, int* count);

KT_API int kt_text_fetch(KtConfig* cfg, const char* id, char* buf, int cch);
KT_API int kt_text_apply_template(const char* tmpl, const char* content, char* buf, int cch);

#ifdef __cplusplus
}
#endif

#endif
