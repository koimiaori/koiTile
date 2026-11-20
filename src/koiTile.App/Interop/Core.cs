using System;
using System.Runtime.InteropServices;
using System.Text;

namespace koiTile.Interop
{
    internal static class Core
    {
        private const string Dll = "koiTileCore.dll";
        private const CallingConvention Cc = CallingConvention.Cdecl;

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_init();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern void kt_shutdown();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern void kt_free(IntPtr p);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_last_error(byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_version(byte[] buf, int cch);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_appdata_dir(byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_path(byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tileassets_dir(byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_temp_dir(byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_size_pixels(int size, ref int width, ref int height);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_size_name(int size, byte[] buf, int cch);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern IntPtr kt_config_new();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern IntPtr kt_config_load(byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_save(IntPtr cfg, byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_save_default(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern void kt_config_free(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_clear(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_tile_count(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_tile_id_at(IntPtr cfg, int index, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_add_tile(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_remove_tile(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_has_tile(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_rename_tile(IntPtr cfg, byte[] id, byte[] newId);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_get(IntPtr cfg, byte[] id, byte[] key, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_set(IntPtr cfg, byte[] id, byte[] key, byte[] value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_get_int(IntPtr cfg, byte[] id, byte[] key, int fallback);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_set_int(IntPtr cfg, byte[] id, byte[] key, int value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_set_board(IntPtr cfg, byte[] key, byte[] value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_get_board(IntPtr cfg, byte[] key, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_get_int_board(IntPtr cfg, byte[] key, int fallback);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_set_int_board(IntPtr cfg, byte[] key, int value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_count(IntPtr cfg, byte[] id, byte[] key);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_get(IntPtr cfg, byte[] id, byte[] key, int index, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_add(IntPtr cfg, byte[] id, byte[] key, byte[] value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_insert(IntPtr cfg, byte[] id, byte[] key, int index, byte[] value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_set(IntPtr cfg, byte[] id, byte[] key, int index, byte[] value);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_remove(IntPtr cfg, byte[] id, byte[] key, int index);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_move(IntPtr cfg, byte[] id, byte[] key, int from, int to);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_list_clear(IntPtr cfg, byte[] id, byte[] key);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_export(IntPtr cfg, byte[] id, byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_export_all(IntPtr cfg, byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_import(IntPtr cfg, byte[] path, byte[] idBuf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_config_copy_tile(IntPtr cfg, byte[] srcId, byte[] dstId);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_config_path(byte[] shortcut, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_config_save_tile(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_config_load_tile(IntPtr cfg, byte[] id);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_file_read_text(byte[] path, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_file_write_text(byte[] path, byte[] text);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_file_exists(byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_file_delete(byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_dir_create(byte[] path);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_dir_list(byte[] dir, byte[] ext, byte[] buf, int cch, ref int count);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_dir_clear(byte[] dir);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_path_join(byte[] a, byte[] b, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_path_dirname(byte[] path, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_path_basename(byte[] path, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_path_exists_dir(byte[] path);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_http_get(byte[] url, byte[] buf, int cch, int timeoutMs);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_xml_build(IntPtr cfg, byte[] id, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_xml_write(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_xml_remove(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_apply(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_restore(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_restore_all(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_state(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_tile_install_start(IntPtr cfg, byte[] id);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_info(byte[] path, ref int width, ref int height);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_resize(byte[] src, int dstW, int dstH, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_cut_grid(byte[] src, int rows, int cols, int cellW, int cellH, byte[] targetName, byte[] outDir, ref int outCount);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_render_tile(IntPtr cfg, byte[] id, int size, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_render_fill(int w, int h, byte[] color, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_render_clear(int w, int h, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_add_label(byte[] src, byte[] label, int fontPx, int margin, int color, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_add_label_box(byte[] src, byte[] line1, byte[] line2, int fontPx, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_render_slice(byte[] src, int canvasW, int canvasH, int imageW, int imageH, int offsetX, int offsetY, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_extract_icon(byte[] src, int index, int iconSize, byte[] dst);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_image_composite(byte[] background, byte[] icon, int iconSize, int offsetX, int offsetY, int dstW, int dstH, byte[] dst);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_shortcut_create(byte[] target, byte[] link, byte[] aumid, byte[] args, byte[] workDir);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_shortcut_aumid(byte[] link, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_shortcut_target_of(byte[] link, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_shell_refresh();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_shell_notify(byte[] path);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_env_explorerpatcher();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_env_win10_start();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_env_tile_notifications();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_env_probe(byte[] buf, int cch);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_reset();
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_sync(IntPtr cfg);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_due(IntPtr cfg, byte[] ids, int cch, ref int count);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_mark(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_seconds_left(IntPtr cfg, byte[] id);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_schedule_due_peek(IntPtr cfg, byte[] ids, int cch, ref int count);

        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_text_fetch(IntPtr cfg, byte[] id, byte[] buf, int cch);
        [DllImport(Dll, CallingConvention = Cc)] internal static extern int kt_text_apply_template(byte[] tmpl, byte[] content, byte[] buf, int cch);

        internal static byte[] Utf8(string value)
        {
            return Encoding.UTF8.GetBytes((value ?? string.Empty) + "\0");
        }

        internal static string Decode(byte[] buffer)
        {
            int n = Array.IndexOf(buffer, (byte)0);
            if (n < 0)
            {
                n = buffer.Length;
            }
            return Encoding.UTF8.GetString(buffer, 0, n);
        }

        internal static string Get(Func<byte[], int, int> call, int size)
        {
            byte[] buffer = new byte[size];
            if (call(buffer, size) != 0)
            {
                return string.Empty;
            }
            return Decode(buffer);
        }

        internal static string GetBuffer(byte[] buffer)
        {
            return Decode(buffer);
        }

        internal static string LastError()
        {
            return Get(kt_last_error, 1024);
        }

        internal static string BoardString(IntPtr cfg, string key)
        {
            byte[] buffer = new byte[2048];
            if (kt_config_get_board(cfg, Utf8(key), buffer, buffer.Length) != 0)
            {
                return string.Empty;
            }
            return Decode(buffer);
        }

        internal static int BoardInt(IntPtr cfg, string key, int fallback)
        {
            return kt_config_get_int_board(cfg, Utf8(key), fallback);
        }

        internal static void SetBoardString(IntPtr cfg, string key, string value)
        {
            kt_config_set_board(cfg, Utf8(key), Utf8(value ?? string.Empty));
        }

        internal static void SetBoardInt(IntPtr cfg, string key, int value)
        {
            kt_config_set_int_board(cfg, Utf8(key), value);
        }
        internal static string ShortcutTarget(string shortcut)
        {
            if (string.IsNullOrWhiteSpace(shortcut))
            {
                return string.Empty;
            }
            byte[] buffer = new byte[1024];
            if (kt_shortcut_target_of(Utf8(shortcut), buffer, buffer.Length) != 0)
            {
                return string.Empty;
            }
            return Decode(buffer);
        }
        internal static string GetBufferString(IntPtr cfg, string id, string key)
        {
            byte[] buffer = new byte[2048];
            if (kt_config_get(cfg, Utf8(id), Utf8(key), buffer, buffer.Length) != 0)
            {
                return string.Empty;
            }
            return Decode(buffer);
        }

        internal static string GetListString(IntPtr cfg, string id, string key, int index)
        {
            byte[] buffer = new byte[2048];
            if (kt_config_list_get(cfg, Utf8(id), Utf8(key), index, buffer, buffer.Length) != 0)
            {
                return string.Empty;
            }
            return Decode(buffer);
        }
    }
}
