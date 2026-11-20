#include "koitile_internal.h"

static int kt_path_same(const char* a, const char* b)
{
    int i;
    if (!a || !b)
    {
        return 0;
    }
    for (i = 0; a[i] && b[i]; i++)
    {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
        {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z')
        {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca == '/')
        {
            ca = '\\';
        }
        if (cb == '/')
        {
            cb = '\\';
        }
        if (ca != cb)
        {
            return 0;
        }
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int kt_size_code(int size, char* buf, int cch)
{
    int width;
    int height;
    int n;
    if (kt_size_pixels(size, &width, &height) != KT_OK)
    {
        kt_err("invalid size");
        return KT_ERR;
    }
    n = sprintf(buf, "%dx%d", width, height);
    if (n < 0 || n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    return KT_OK;
}

static int kt_state_path(char* buf, int cch)
{
    char app[KT_MAX_PATHBUF];
    if (kt_appdata_dir(app, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_path_join(app, "state.json", buf, cch);
}

KtState* kt_state_load(void)
{
    KtState* st;
    KtJson* root;
    KtJson* tiles;
    char path[KT_MAX_PATHBUF];
    KtJsonMember* m;
    st = (KtState*)calloc(1, sizeof(KtState));
    if (!st)
    {
        kt_err("out of memory");
        return NULL;
    }
    if (kt_state_path(path, KT_MAX_PATHBUF) != KT_OK)
    {
        return st;
    }
    if (!kt_file_exists(path))
    {
        return st;
    }
    root = kt_json_parse_file(path);
    if (!root)
    {
        return st;
    }
    tiles = kt_json_get(root, "tiles");
    if (tiles && tiles->type == KT_JSON_OBJECT)
    {
        m = tiles->members;
        while (m)
        {
            KtStateTile* t;
            KtJson* v = m->value;
            if (!v || v->type != KT_JSON_OBJECT)
            {
                m = m->next;
                continue;
            }
            t = kt_state_find(st, m->key, 1);
            if (!t)
            {
                m = m->next;
                continue;
            }
            t->applied = kt_json_get_int(v, "applied", 0);
            free(t->manifest);
            t->manifest = NULL;
            {
                const char* manifest = kt_json_get_string(v, "manifest", NULL);
                if (manifest)
                {
                    t->manifest = kt_strdup(manifest);
                }
            }
            free(t->orig_target);
            t->orig_target = NULL;
            free(t->orig_args);
            t->orig_args = NULL;
            free(t->orig_work_dir);
            t->orig_work_dir = NULL;
            free(t->created_shortcut);
            t->created_shortcut = NULL;
            {
                const char* created = kt_json_get_string(v, "createdShortcut", NULL);
                if (created)
                {
                    t->created_shortcut = kt_strdup(created);
                }
            }
            {
                const char* origin = kt_json_get_string(v, "origTarget", NULL);
                const char* origin_args = kt_json_get_string(v, "origArgs", NULL);
                const char* origin_dir = kt_json_get_string(v, "origWorkDir", NULL);
                if (origin)
                {
                    t->orig_target = kt_strdup(origin);
                }
                if (origin_args)
                {
                    t->orig_args = kt_strdup(origin_args);
                }
                if (origin_dir)
                {
                    t->orig_work_dir = kt_strdup(origin_dir);
                }
            }
            kt_file_list_free(t->files, t->file_count);
            t->files = NULL;
            t->file_count = 0;
            {
                KtJson* files = kt_json_get(v, "files");
                int i;
                if (files && files->type == KT_JSON_ARRAY)
                {
                    for (i = 0; i < files->count; i++)
                    {
                        KtJson* item = files->items[i];
                        if (item && item->type == KT_JSON_STRING && item->text)
                        {
                            kt_file_list_add(&t->files, &t->file_count, item->text);
                        }
                    }
                }
            }
            t->installed = kt_json_get_int(v, "installed", 0);
            free(t->install_dir);
            t->install_dir = NULL;
            free(t->install_manifest);
            t->install_manifest = NULL;
            {
                const char* dir = kt_json_get_string(v, "installDir", NULL);
                const char* manifest = kt_json_get_string(v, "installManifest", NULL);
                if (dir && dir[0])
                {
                    t->install_dir = kt_strdup(dir);
                }
                if (manifest && manifest[0])
                {
                    t->install_manifest = kt_strdup(manifest);
                }
            }
            kt_file_list_free(t->install_files, t->install_file_count);
            t->install_files = NULL;
            t->install_file_count = 0;
            {
                KtJson* files = kt_json_get(v, "installFiles");
                int i;
                if (files && files->type == KT_JSON_ARRAY)
                {
                    for (i = 0; i < files->count; i++)
                    {
                        KtJson* item = files->items[i];
                        if (item && item->type == KT_JSON_STRING && item->text)
                        {
                            kt_file_list_add(&t->install_files, &t->install_file_count, item->text);
                        }
                    }
                }
            }
            m = m->next;
        }
    }
    kt_json_free(root);
    return st;
}

void kt_state_free(KtState* st)
{
    KtStateTile* t;
    KtStateTile* next;
    if (!st)
    {
        return;
    }
    t = st->head;
    while (t)
    {
        next = t->next;
        free(t->id);
        free(t->manifest);
        free(t->orig_target);
        free(t->orig_args);
        free(t->orig_work_dir);
        free(t->created_shortcut);
        kt_file_list_free(t->files, t->file_count);
        free(t->install_dir);
        free(t->install_manifest);
        kt_file_list_free(t->install_files, t->install_file_count);
        free(t);
        t = next;
    }
    free(st);
}

KtStateTile* kt_state_find(KtState* st, const char* id, int create)
{
    KtStateTile* t;
    if (!st || !id || !id[0])
    {
        return NULL;
    }
    t = st->head;
    while (t)
    {
        if (kt_streq(t->id, id))
        {
            return t;
        }
        t = t->next;
    }
    if (!create)
    {
        return NULL;
    }
    t = (KtStateTile*)calloc(1, sizeof(KtStateTile));
    if (!t)
    {
        kt_err("out of memory");
        return NULL;
    }
    t->id = kt_strdup(id);
    if (!t->id)
    {
        free(t);
        kt_err("out of memory");
        return NULL;
    }
    if (st->tail)
    {
        st->tail->next = t;
    }
    else
    {
        st->head = t;
    }
    st->tail = t;
    return t;
}

void kt_state_drop(KtStateTile* t)
{
    KtStateTile* p;
    if (!t)
    {
        return;
    }
    p = (KtStateTile*)t;
    free(p->id);
    free(p->manifest);
    free(p->orig_target);
    free(p->orig_args);
    free(p->orig_work_dir);
    free(p->created_shortcut);
    kt_file_list_free(p->files, p->file_count);
    free(p->install_dir);
    free(p->install_manifest);
    kt_file_list_free(p->install_files, p->install_file_count);
    free(p);
}

int kt_state_set_origin(KtStateTile* t, const char* target, const char* args, const char* work_dir)
{
    if (!t)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (t->orig_target)
    {
        return KT_OK;
    }
    t->orig_target = kt_strdup(target ? target : "");
    t->orig_args = kt_strdup(args ? args : "");
    t->orig_work_dir = kt_strdup(work_dir ? work_dir : "");
    if (!t->orig_target || !t->orig_args || !t->orig_work_dir)
    {
        free(t->orig_target);
        free(t->orig_args);
        free(t->orig_work_dir);
        t->orig_target = NULL;
        t->orig_args = NULL;
        t->orig_work_dir = NULL;
        kt_err("out of memory");
        return KT_ERR;
    }
    return KT_OK;
}

static void kt_state_unlink(KtState* st, KtStateTile* t)
{
    KtStateTile* p;
    KtStateTile* prev = NULL;
    if (!st || !t)
    {
        return;
    }
    p = st->head;
    while (p)
    {
        if (p == t)
        {
            if (prev)
            {
                prev->next = p->next;
            }
            else
            {
                st->head = p->next;
            }
            if (st->tail == p)
            {
                st->tail = prev;
            }
            return;
        }
        prev = p;
        p = p->next;
    }
}

int kt_state_save(KtState* st)
{
    KtJson* root;
    KtJson* tiles;
    KtStateTile* t;
    char path[KT_MAX_PATHBUF];
    char* text;
    int rc;
    if (!st)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_state_path(path, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    root = kt_json_new_object();
    if (!root)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    tiles = kt_json_new_object();
    if (!tiles)
    {
        kt_json_free(root);
        kt_err("out of memory");
        return KT_ERR;
    }
    t = st->head;
    while (t)
    {
        KtJson* v = kt_json_new_object();
        KtJson* files = kt_json_new_array();
        KtJson* install_files = kt_json_new_array();
        int i;
        if (!v || !files || !install_files)
        {
            kt_json_free(v);
            kt_json_free(files);
            kt_json_free(install_files);
            kt_json_free(root);
            kt_err("out of memory");
            return KT_ERR;
        }
        kt_json_set_int(v, "applied", t->applied);
        kt_json_set_string(v, "manifest", t->manifest ? t->manifest : "");
        if (t->orig_target)
        {
            kt_json_set_string(v, "origTarget", t->orig_target);
            kt_json_set_string(v, "origArgs", t->orig_args ? t->orig_args : "");
            kt_json_set_string(v, "origWorkDir", t->orig_work_dir ? t->orig_work_dir : "");
        }
        if (t->created_shortcut && t->created_shortcut[0])
        {
            kt_json_set_string(v, "createdShortcut", t->created_shortcut);
        }
        for (i = 0; i < t->file_count; i++)
        {
            kt_json_add(files, kt_json_new_string(t->files[i]));
        }
        kt_json_set(v, "files", files);
        kt_json_set_int(v, "installed", t->installed);
        kt_json_set_string(v, "installDir", t->install_dir ? t->install_dir : "");
        kt_json_set_string(v, "installManifest", t->install_manifest ? t->install_manifest : "");
        for (i = 0; i < t->install_file_count; i++)
        {
            kt_json_add(install_files, kt_json_new_string(t->install_files[i]));
        }
        kt_json_set(v, "installFiles", install_files);
        kt_json_set(tiles, t->id, v);
        t = t->next;
    }
    kt_json_set(root, "tiles", tiles);
    text = kt_json_dump(root);
    kt_json_free(root);
    if (!text)
    {
        kt_err("serialize failed");
        return KT_ERR;
    }
    rc = kt_file_write_text(path, text);
    free(text);
    return rc;
}

static int kt_tile_shortcut(KtConfig* cfg, const char* id, char* buf, int cch)
{
    if (!cfg || !id)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_config_get(cfg, id, "shortcut", buf, cch) != KT_OK)
    {
        kt_err("tile shortcut unavailable");
        return KT_ERR;
    }
    if (!buf[0])
    {
        kt_err("tile shortcut is empty");
        return KT_ERR;
    }
    return KT_OK;
}

static void kt_apply_notify(const char* dir, const char* manifest)
{
    if (dir && dir[0])
    {
        kt_shell_notify(dir);
    }
    if (manifest && manifest[0])
    {
        kt_shell_notify(manifest);
    }
    kt_shell_refresh();
}

int kt_xml_write(KtConfig* cfg, const char* id)
{
    char shortcut[KT_MAX_PATHBUF];
    char manifest[KT_MAX_PATHBUF];
    char dir[KT_MAX_PATHBUF];
    char xml[KT_MAX_TEXT];
    KtState* st;
    KtStateTile* t;
    int changed = 0;
    int rc = KT_ERR;
    if (kt_tile_shortcut(cfg, id, shortcut, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_manifest_path(shortcut, manifest, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_xml_build(cfg, id, xml, KT_MAX_TEXT) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_file_write_text(manifest, xml) != KT_OK)
    {
        return KT_ERR;
    }
    st = kt_state_load();
    if (!st)
    {
        return KT_ERR;
    }
    t = kt_state_find(st, id, 1);
    if (!t)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    if (!t->manifest || !kt_streq(t->manifest, manifest))
    {
        free(t->manifest);
        t->manifest = kt_strdup(manifest);
        changed = 1;
    }
    if (kt_state_save(st) != KT_OK)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    kt_state_free(st);
    if (changed)
    {
        if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
        {
            dir[0] = '\0';
        }
        kt_apply_notify(dir, manifest);
    }
    rc = KT_OK;
    return rc;
}

int kt_xml_remove(KtConfig* cfg, const char* id)
{
    char shortcut[KT_MAX_PATHBUF];
    char manifest[KT_MAX_PATHBUF];
    char dir[KT_MAX_PATHBUF];
    int existed = 0;
    KtState* st;
    KtStateTile* t;
    if (kt_tile_shortcut(cfg, id, shortcut, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_manifest_path(shortcut, manifest, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
    {
        dir[0] = '\0';
    }
    if (kt_file_exists(manifest))
    {
        if (kt_file_delete(manifest) == KT_OK)
        {
            existed = 1;
        }
    }
    st = kt_state_load();
    if (st)
    {
        t = kt_state_find(st, id, 0);
        if (t)
        {
            free(t->manifest);
            t->manifest = NULL;
            kt_state_save(st);
        }
        kt_state_free(st);
    }
    if (existed)
    {
        kt_apply_notify(dir, manifest);
    }
    return KT_OK;
}

int kt_tile_apply_size(KtConfig* cfg, const char* id, int size, char* out, int cch)
{
    char shortcut[KT_MAX_PATHBUF];
    char dir[KT_MAX_PATHBUF];
    char target[KT_MAX_PATHBUF];
    char code[32];
    KtTile* tile;
    int n;
    if (!cfg || !id || !out || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    out[0] = '\0';
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_tile_shortcut(cfg, id, shortcut, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_tile_file_target(cfg, tile, target, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_size_code(size, code, sizeof(code)) != KT_OK)
    {
        return KT_ERR;
    }
    n = sprintf(out, "%s\\%s_%s.png", dir, target, code);
    if (n < 0 || n + 1 > cch)
    {
        out[0] = '\0';
        kt_err("buffer too small");
        return KT_ERR;
    }
    return KT_OK;
}

int kt_tile_apply(KtConfig* cfg, const char* id)
{
    char shortcut[KT_MAX_PATHBUF];
    char dir[KT_MAX_PATHBUF];
    char assets[KT_MAX_PATHBUF];
    char manifest[KT_MAX_PATHBUF];
    char rendered[4][KT_MAX_PATHBUF];
    KtState* st;
    KtStateTile* t;
    int rendered_count[4];
    int size;
    int i;
    int rc = KT_ERR;
    int had_manifest = 0;
    char** files = NULL;
    int file_count = 0;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (!kt_config_has_tile(cfg, id))
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_tile_shortcut(cfg, id, shortcut, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_manifest_path(shortcut, manifest, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_dir_create(dir) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_tileassets_dir(assets, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_dir_create(assets) != KT_OK)
    {
        return KT_ERR;
    }
    for (size = 0; size < 4; size++)
    {
        rendered_count[size] = 0;
        rendered[size][0] = '\0';
        if (kt_tile_apply_size(cfg, id, size, rendered[size], KT_MAX_PATHBUF) != KT_OK)
        {
            continue;
        }
        if (kt_image_render_tile(cfg, id, size, rendered[size]) == KT_OK)
        {
            rendered_count[size] = 1;
            if (kt_file_list_add(&files, &file_count, rendered[size]) != KT_OK)
            {
                kt_file_list_free(files, file_count);
                return KT_ERR;
            }
        }
    }
    st = kt_state_load();
    if (!st)
    {
        kt_file_list_free(files, file_count);
        return KT_ERR;
    }
    t = kt_state_find(st, id, 1);
    if (!t)
    {
        kt_state_free(st);
        kt_file_list_free(files, file_count);
        return KT_ERR;
    }
    kt_file_list_free(t->files, t->file_count);
    t->files = NULL;
    t->file_count = 0;
    for (i = 0; i < file_count; i++)
    {
        kt_file_list_add(&t->files, &t->file_count, files[i]);
    }
    kt_file_list_free(files, file_count);
    had_manifest = kt_file_exists(manifest);
    {
        char xml[KT_MAX_TEXT];
        if (kt_xml_build(cfg, id, xml, KT_MAX_TEXT) != KT_OK)
        {
            kt_state_free(st);
            return KT_ERR;
        }
        if (kt_file_write_text(manifest, xml) != KT_OK)
        {
            kt_state_free(st);
            return KT_ERR;
        }
    }
    free(t->manifest);
    t->manifest = kt_strdup(manifest);
    t->applied = 1;
    {
        KtTile* tile = kt_config_find(cfg, id);
        KtCfgEntry* le = tile ? kt_tile_field(tile, "link", 0) : NULL;
        if (le && le->value && le->value[0])
        {
            if (!kt_file_exists(shortcut))
            {
                kt_err("shortcut not found");
                kt_state_free(st);
                return KT_ERR;
            }
            if (!t->orig_target)
            {
                char o_target[KT_MAX_PATHBUF];
                char o_args[KT_MAX_PATHBUF];
                char o_dir[KT_MAX_PATHBUF];
                if (kt_shortcut_target(shortcut, o_target, KT_MAX_PATHBUF) != KT_OK)
                {
                    kt_state_free(st);
                    return KT_ERR;
                }
                if (kt_shortcut_args(shortcut, o_args, KT_MAX_PATHBUF) != KT_OK)
                {
                    o_args[0] = '\0';
                }
                if (kt_shortcut_work_dir(shortcut, o_dir, KT_MAX_PATHBUF) != KT_OK)
                {
                    o_dir[0] = '\0';
                }
                if (kt_state_set_origin(t, o_target, o_args, o_dir) != KT_OK)
                {
                    kt_state_free(st);
                    return KT_ERR;
                }
            }
            {
                char aumid[512];
                if (kt_shortcut_aumid(shortcut, aumid, (int)sizeof(aumid)) != KT_OK)
                {
                    aumid[0] = '\0';
                }
                if (kt_shortcut_create(le->value, shortcut, aumid, "",
                                       t->orig_work_dir && t->orig_work_dir[0] ? t->orig_work_dir
                                                                               : NULL) != KT_OK)
                {
                    kt_state_free(st);
                    return KT_ERR;
                }
            }
        }
    }
    if (kt_state_save(st) != KT_OK)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    kt_state_free(st);
    for (size = 0; size < 4; size++)
    {
        if (rendered_count[size])
        {
            kt_shell_notify(rendered[size]);
        }
    }
    kt_apply_notify(dir, had_manifest ? NULL : manifest);
    if (had_manifest)
    {
        kt_shell_notify(manifest);
    }
    rc = KT_OK;
    return rc;
}

static int kt_install_cleanup(KtStateTile* t)
{
    int removed = 0;
    int i;
    if (!t)
    {
        return 0;
    }
    for (i = 0; i < t->install_file_count; i++)
    {
        if (t->install_files[i] && kt_file_exists(t->install_files[i]))
        {
            char parent[KT_MAX_PATHBUF];
            size_t len = strlen(t->install_files[i]);
            int is_link = len > 4 && strcmp(t->install_files[i] + len - 4, ".lnk") == 0;
            kt_file_delete(t->install_files[i]);
            removed = 1;
            if (is_link && kt_path_dirname(t->install_files[i], parent, KT_MAX_PATHBUF) == KT_OK &&
                kt_dir_empty(parent))
            {
                kt_file_delete(parent);
            }
        }
    }
    if (t->install_manifest && t->install_manifest[0] && kt_file_exists(t->install_manifest))
    {
        kt_file_delete(t->install_manifest);
        removed = 1;
    }
    if (t->install_dir && t->install_dir[0] && kt_dir_empty(t->install_dir))
    {
        if (kt_file_delete(t->install_dir) == KT_OK)
        {
            removed = 1;
        }
    }
    return removed;
}

int kt_tile_restore(KtConfig* cfg, const char* id)
{
    char shortcut[KT_MAX_PATHBUF];
    char dir[KT_MAX_PATHBUF];
    char manifest[KT_MAX_PATHBUF];
    char tile_config[KT_MAX_PATHBUF];
    char created[KT_MAX_PATHBUF];
    int removed = 0;
    KtState* st;
    KtStateTile* t;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (!kt_config_has_tile(cfg, id))
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    created[0] = '\0';
    tile_config[0] = '\0';
    if (kt_config_get(cfg, id, "shortcut", shortcut, KT_MAX_PATHBUF) != KT_OK || !shortcut[0])
    {
        shortcut[0] = '\0';
        dir[0] = '\0';
        manifest[0] = '\0';
    }
    else
    {
        kt_tile_config_path(shortcut, tile_config, KT_MAX_PATHBUF);
        if (kt_path_dirname(shortcut, dir, KT_MAX_PATHBUF) != KT_OK)
        {
            dir[0] = '\0';
        }
        if (kt_manifest_path(shortcut, manifest, KT_MAX_PATHBUF) != KT_OK)
        {
            manifest[0] = '\0';
        }
    }
    st = kt_state_load();
    if (!st)
    {
        return KT_ERR;
    }
    t = kt_state_find(st, id, 0);
    if (t)
    {
        int i;
        if (t->created_shortcut && t->created_shortcut[0] &&
            (int)strlen(t->created_shortcut) + 1 <= KT_MAX_PATHBUF)
        {
            memcpy(created, t->created_shortcut, strlen(t->created_shortcut) + 1);
            if (kt_file_exists(created))
            {
                kt_file_delete(created);
                removed = 1;
            }
        }
        for (i = 0; i < t->file_count; i++)
        {
            if (kt_file_exists(t->files[i]))
            {
                kt_file_delete(t->files[i]);
                removed = 1;
            }
        }
        if (t->manifest && t->manifest[0] && kt_file_exists(t->manifest))
        {
            kt_file_delete(t->manifest);
            removed = 1;
        }
        if (kt_install_cleanup(t))
        {
            removed = 1;
        }
        if (t->orig_target && t->orig_target[0] && shortcut[0] && kt_file_exists(shortcut) &&
            !(created[0] && kt_path_same(created, shortcut)))
        {
            char aumid[512];
            int restore_rc;
            if (kt_shortcut_aumid(shortcut, aumid, (int)sizeof(aumid)) != KT_OK)
            {
                aumid[0] = '\0';
            }
            restore_rc = kt_shortcut_create(t->orig_target, shortcut, aumid,
                                            t->orig_args ? t->orig_args : "",
                                            t->orig_work_dir && t->orig_work_dir[0]
                                                ? t->orig_work_dir
                                                : NULL);
            if (restore_rc != KT_OK)
            {
                kt_state_free(st);
                return KT_ERR;
            }
        }
    }
    if (manifest[0] && kt_file_exists(manifest))
    {
        kt_file_delete(manifest);
        removed = 1;
    }
    if (tile_config[0] && kt_file_exists(tile_config))
    {
        kt_file_delete(tile_config);
        removed = 1;
    }
    if (t)
    {
        kt_state_unlink(st, t);
        kt_state_drop(t);
    }
    kt_state_save(st);
    kt_state_free(st);
    if (removed || dir[0])
    {
        kt_apply_notify(dir, manifest);
    }
    return KT_OK;
}

int kt_tile_restore_all(KtConfig* cfg)
{
    int total;
    int i;
    int rc = KT_OK;
    if (!cfg)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    total = kt_config_tile_count(cfg);
    for (i = 0; i < total; i++)
    {
        char id[256];
        if (kt_config_tile_id_at(cfg, i, id, 256) != KT_OK)
        {
            continue;
        }
        if (kt_tile_restore(cfg, id) != KT_OK)
        {
            rc = KT_ERR;
        }
    }
    return rc;
}

int kt_tile_install_start(KtConfig* cfg, const char* id)
{
    char shortcut[KT_MAX_PATHBUF];
    char target[KT_MAX_PATHBUF];
    char exe_dir[KT_MAX_PATHBUF];
    char assets[KT_MAX_PATHBUF];
    char manifest[KT_MAX_PATHBUF];
    char manifest_named[KT_MAX_PATHBUF];
    char name[KT_MAX_PATHBUF];
    char rendered[4][KT_MAX_PATHBUF];
    char xml[KT_MAX_TEXT];
    KtTile* tile;
    KtState* st;
    KtStateTile* t;
    char** files = NULL;
    int file_count = 0;
    int size;
    int i;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (!kt_config_has_tile(cfg, id))
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_config_get(cfg, id, "shortcut", shortcut, KT_MAX_PATHBUF) != KT_OK || !shortcut[0])
    {
        kt_err("tile shortcut is empty");
        return KT_ERR;
    }
    if (!kt_file_exists(shortcut))
    {
        kt_err("shortcut not found");
        return KT_ERR;
    }
    {
        size_t link_len = strlen(shortcut);
        int is_link = link_len > 4 && _stricmp(shortcut + link_len - 4, ".lnk") == 0;
        if (is_link)
        {
            if (kt_shortcut_target(shortcut, target, KT_MAX_PATHBUF) != KT_OK)
            {
                return KT_ERR;
            }
        }
        else
        {
            if ((int)link_len + 1 > KT_MAX_PATHBUF)
            {
                kt_err("shortcut path too long");
                return KT_ERR;
            }
            memcpy(target, shortcut, link_len + 1);
        }
    }
    if (!target[0])
    {
        kt_err("shortcut target is empty");
        return KT_ERR;
    }
    if (!kt_path_is_local(target))
    {
        kt_err("shortcut target is not a local path");
        return KT_ERR;
    }
    if (!kt_file_exists(target))
    {
        kt_err("shortcut target not found");
        return KT_ERR;
    }
    if (kt_path_dirname(target, exe_dir, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_join(exe_dir, "VisualElements", assets, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_join(exe_dir, "VisualElementsManifest.xml", manifest, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_path_basename(target, name, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    {
        char* dot = strrchr(name, '.');
        if (dot && dot != name)
        {
            *dot = '\0';
        }
    }
    {
        char leaf[KT_MAX_PATHBUF];
        int n = snprintf(leaf, KT_MAX_PATHBUF, "%s.VisualElementsManifest.xml", name);
        if (n < 0 || n >= KT_MAX_PATHBUF)
        {
            kt_err("buffer too small");
            return KT_ERR;
        }
        if (kt_path_join(exe_dir, leaf, manifest_named, KT_MAX_PATHBUF) != KT_OK)
        {
            return KT_ERR;
        }
    }
    tile = kt_config_find(cfg, id);
    if (kt_tile_file_target(cfg, tile, name, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    st = kt_state_load();
    if (!st)
    {
        return KT_ERR;
    }
    t = kt_state_find(st, id, 1);
    if (!t)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    kt_install_cleanup(t);
    if (kt_dir_create(assets) != KT_OK)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    for (size = 0; size < 4; size++)
    {
        int width;
        int height;
        int n;
        rendered[size][0] = '\0';
        if (kt_size_pixels(size, &width, &height) != KT_OK)
        {
            continue;
        }
        n = sprintf(rendered[size], "%s\\%s_%dx%d.png", assets, name, width, height);
        if (n < 0 || n + 1 > KT_MAX_PATHBUF)
        {
            rendered[size][0] = '\0';
            kt_file_list_free(files, file_count);
            kt_state_free(st);
            kt_err("buffer too small");
            return KT_ERR;
        }
        if (kt_image_render_tile(cfg, id, size, rendered[size]) == KT_OK)
        {
            if (kt_file_list_add(&files, &file_count, rendered[size]) != KT_OK)
            {
                kt_file_list_free(files, file_count);
                kt_state_free(st);
                return KT_ERR;
            }
        }
    }
    if (file_count == 0)
    {
        kt_file_list_free(files, file_count);
        kt_state_free(st);
        if (kt_dir_empty(assets))
        {
            kt_file_delete(assets);
        }
        kt_err("render tile images failed");
        return KT_ERR;
    }
    if (kt_xml_build_prefixed(cfg, id, "VisualElements\\", xml, KT_MAX_TEXT) != KT_OK)
    {
        kt_file_list_free(files, file_count);
        kt_state_free(st);
        return KT_ERR;
    }
    if (kt_file_write_text(manifest, xml) != KT_OK)
    {
        kt_file_list_free(files, file_count);
        kt_state_free(st);
        return KT_ERR;
    }
    if (kt_file_write_text(manifest_named, xml) != KT_OK)
    {
        kt_file_list_free(files, file_count);
        kt_state_free(st);
        return KT_ERR;
    }
    if (kt_file_list_add(&files, &file_count, manifest_named) != KT_OK)
    {
        kt_file_list_free(files, file_count);
        kt_state_free(st);
        return KT_ERR;
    }
    {
        char apps[KT_MAX_PATHBUF];
        char link[KT_MAX_PATHBUF];
        int n;
        if (kt_start_apps_dir(apps, KT_MAX_PATHBUF) == KT_OK)
        {
            char link_name[KT_MAX_PATHBUF];
            char app_name[KT_MAX_PATHBUF];
            char* cut;
            memcpy(link_name, name, strlen(name) + 1);
            if (kt_config_get(cfg, id, "appName", app_name, KT_MAX_PATHBUF) == KT_OK && app_name[0])
            {
                cut = app_name;
                while (*cut)
                {
                    if (*cut == '\\' || *cut == '/' || *cut == ':' || *cut == '*' || *cut == '?' || *cut == '"' || *cut == '<' || *cut == '>' || *cut == '|')
                    {
                        *cut = '-';
                    }
                    cut++;
                }
                memcpy(link_name, app_name, strlen(app_name) + 1);
            }
            n = snprintf(link, KT_MAX_PATHBUF, "%s\\%s.lnk", apps, link_name);
            if (n > 0 && n < KT_MAX_PATHBUF && kt_shortcut_create(target, link, NULL, NULL, exe_dir) == KT_OK)
            {
                if (kt_file_list_add(&files, &file_count, link) != KT_OK)
                {
                    kt_file_list_free(files, file_count);
                    kt_state_free(st);
                    return KT_ERR;
                }
            }
        }
    }
    {
        char* dir_copy = kt_strdup(assets);
        char* manifest_copy = kt_strdup(manifest);
        if (!dir_copy || !manifest_copy)
        {
            free(dir_copy);
            free(manifest_copy);
            kt_file_list_free(files, file_count);
            kt_state_free(st);
            kt_err("out of memory");
            return KT_ERR;
        }
        free(t->install_dir);
        t->install_dir = dir_copy;
        free(t->install_manifest);
        t->install_manifest = manifest_copy;
    }
    kt_file_list_free(t->install_files, t->install_file_count);
    t->install_files = NULL;
    t->install_file_count = 0;
    for (i = 0; i < file_count; i++)
    {
        kt_file_list_add(&t->install_files, &t->install_file_count, files[i]);
    }
    kt_file_list_free(files, file_count);
    t->installed = 1;
    if (kt_state_save(st) != KT_OK)
    {
        kt_state_free(st);
        return KT_ERR;
    }
    kt_state_free(st);
    kt_shell_notify(exe_dir);
    kt_shell_refresh();
    return KT_OK;
}

int kt_tile_state(KtConfig* cfg, const char* id)
{
    KtState* st;
    KtStateTile* t;
    int applied = 0;
    if (!cfg || !id || !id[0])
    {
        return 0;
    }
    st = kt_state_load();
    if (!st)
    {
        return 0;
    }
    t = kt_state_find(st, id, 0);
    if (t && t->applied)
    {
        applied = 1;
    }
    kt_state_free(st);
    return applied;
}
