#include "koitile_internal.h"

#define KT_CONFIG_SUFFIX ".koitileconfig.json"

typedef struct KtDefault
{
    const char* key;
    const char* value;
} KtDefault;

static const KtDefault kt_defaults[] = {
    {"name", ""},
    {"shortcut", ""},
    {"size", "3"},
    {"colorEnabled", "0"},
    {"color", "#2D89EF"},
    {"image", ""},
    {"imageOffsetX", "0"},
    {"imageOffsetY", "0"},
    {"imageScale", "100"},
    {"title", ""},
    {"subtitle", ""},
    {"appName", ""},
    {"link", ""},
    {"textSource", "0"},
    {"textSourcePath", ""},
    {"refreshSeconds", "30"},
    {"textTemplate", "{content}"},
    {"iconEnabled", "0"},
    {"iconPath", ""},
    {"iconSize", "64"},
    {"iconOffsetX", "0"},
    {"iconOffsetY", "0"},
    {"peekSeconds", "12"},
    {"showName", "1"},
    {"numberAll", "0"},
    {"autoCrop", "1"},
    {"updateMode", "0"},
    {"planMinutes", "30"},
    {"triggerType", "0"},
    {"triggerTimeKind", "0"},
    {"triggerMinutes", "30"},
    {"triggerSystemType", "6"},
    {"cropX0", "0"},
    {"cropY0", "0"},
    {"cropX1", "0"},
    {"cropY1", "0"},
    {"cropX2", "0"},
    {"cropY2", "0"},
    {"cropX3", "0"},
    {"cropY3", "0"},
    {"previewSize", "3"},
    {"cropZoom0", "0"},
    {"cropZoom1", "0"},
    {"cropZoom2", "0"},
    {"cropZoom3", "0"},
    {"foregroundLight", "1"},
    {"gridCol", "0"},
    {"gridRow", "0"}
};

static const int kt_default_count = (int)(sizeof(kt_defaults) / sizeof(kt_defaults[0]));

static const char* const kt_int_keys[] = {
    "size",
    "colorEnabled",
    "imageOffsetX",
    "imageOffsetY",
    "imageScale",
    "textSource",
    "refreshSeconds",
    "iconEnabled",
    "iconSize",
    "iconOffsetX",
    "iconOffsetY",
    "peekSeconds",
    "showName",
    "numberAll",
    "autoCrop",
    "updateMode",
    "planMinutes",
    "triggerType",
    "triggerTimeKind",
    "triggerMinutes",
    "triggerSystemType",
    "cropX0",
    "cropY0",
    "cropX1",
    "cropY1",
    "cropX2",
    "cropY2",
    "cropX3",
    "cropY3",
    "previewSize",
    "cropZoom0",
    "cropZoom1",
    "cropZoom2",
    "cropZoom3",
    "foregroundLight",
    "gridCol",
    "gridRow"
};

static int kt_is_int_key(const char* key)
{
    int i;
    if (!key)
    {
        return 0;
    }
    for (i = 0; i < (int)(sizeof(kt_int_keys) / sizeof(kt_int_keys[0])); i++)
    {
        if (kt_streq(key, kt_int_keys[i]))
        {
            return 1;
        }
    }
    return 0;
}

static void kt_entry_free(KtCfgEntry* e)
{
    free(e->key);
    free(e->value);
    free(e);
}

KtCfgEntry* kt_tile_field(KtTile* tile, const char* key, int create)
{
    KtCfgEntry* e;
    if (!tile || !key)
    {
        return NULL;
    }
    e = tile->fields.head;
    while (e)
    {
        if (kt_streq(e->key, key))
        {
            return e;
        }
        e = e->next;
    }
    if (!create)
    {
        return NULL;
    }
    e = (KtCfgEntry*)calloc(1, sizeof(KtCfgEntry));
    if (!e)
    {
        return NULL;
    }
    e->key = kt_strdup(key);
    e->value = kt_strdup("");
    if (!e->key || !e->value)
    {
        kt_entry_free(e);
        return NULL;
    }
    if (tile->fields.tail)
    {
        tile->fields.tail->next = e;
    }
    else
    {
        tile->fields.head = e;
    }
    tile->fields.tail = e;
    tile->fields.count++;
    return e;
}

static KtTile* kt_tile_new(const char* id)
{
    KtTile* tile;
    int i;
    if (!id || !id[0])
    {
        return NULL;
    }
    tile = (KtTile*)calloc(1, sizeof(KtTile));
    if (!tile)
    {
        return NULL;
    }
    tile->id = kt_strdup(id);
    if (!tile->id)
    {
        free(tile);
        return NULL;
    }
    for (i = 0; i < kt_default_count; i++)
    {
        KtCfgEntry* e = kt_tile_field(tile, kt_defaults[i].key, 1);
        if (!e)
        {
            continue;
        }
        free(e->value);
        e->value = kt_strdup(kt_defaults[i].value);
    }
    kt_tile_field(tile, "peek", 1);
    kt_tile_field(tile, "queue", 1);
    kt_tile_field(tile, "texts", 1);
    return tile;
}

void kt_tile_free(KtTile* tile)
{
    KtCfgEntry* e;
    KtCfgEntry* next;
    if (!tile)
    {
        return;
    }
    free(tile->id);
    e = tile->fields.head;
    while (e)
    {
        next = e->next;
        kt_entry_free(e);
        e = next;
    }
    free(tile);
}

KtTile* kt_config_find(KtConfig* cfg, const char* id)
{
    int i;
    if (!cfg || !id || !id[0])
    {
        return NULL;
    }
    for (i = 0; i < cfg->count; i++)
    {
        if (cfg->tiles[i] && kt_streq(cfg->tiles[i]->id, id))
        {
            return cfg->tiles[i];
        }
    }
    return NULL;
}

static int kt_config_push(KtConfig* cfg, KtTile* tile)
{
    KtTile** grown;
    int cap;
    if (cfg->count >= cfg->capacity)
    {
        cap = cfg->capacity ? cfg->capacity * 2 : 8;
        grown = (KtTile**)realloc(cfg->tiles, (size_t)cap * sizeof(KtTile*));
        if (!grown)
        {
            kt_err("out of memory");
            return KT_ERR;
        }
        cfg->tiles = grown;
        cfg->capacity = cap;
    }
    cfg->tiles[cfg->count] = tile;
    cfg->count++;
    return KT_OK;
}

void kt_config_remove_at(KtConfig* cfg, int index)
{
    int i;
    if (!cfg || index < 0 || index >= cfg->count)
    {
        return;
    }
    kt_tile_free(cfg->tiles[index]);
    for (i = index; i + 1 < cfg->count; i++)
    {
        cfg->tiles[i] = cfg->tiles[i + 1];
    }
    cfg->count--;
    cfg->tiles[cfg->count] = NULL;
}

KtJson* kt_list_json(KtConfig* cfg, const char* id, const char* key, int create)
{
    KtTile* tile;
    KtCfgEntry* e;
    KtJson* v;
    if (!cfg || !id || !key)
    {
        return NULL;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        return NULL;
    }
    e = kt_tile_field(tile, key, create);
    if (!e)
    {
        return NULL;
    }
    if (!e->value)
    {
        if (!create)
        {
            return NULL;
        }
        v = kt_json_new_array();
        if (!v)
        {
            return NULL;
        }
        e->value = kt_json_dump(v);
        kt_json_free(v);
        return kt_json_parse(e->value ? e->value : "[]");
    }
    v = kt_json_parse(e->value);
    if (!v)
    {
        v = kt_json_new_array();
        if (!v)
        {
            return NULL;
        }
        kt_json_set_string(NULL, NULL, NULL);
        free(e->value);
        e->value = kt_strdup("[]");
        return v;
    }
    if (v->type != KT_JSON_ARRAY)
    {
        KtJson* arr = kt_json_new_array();
        if (arr)
        {
            kt_json_add(arr, v);
            v = arr;
        }
    }
    return v;
}

static int kt_list_store(KtConfig* cfg, const char* id, const char* key, KtJson* arr)
{
    KtTile* tile;
    KtCfgEntry* e;
    char* text;
    if (!cfg || !id || !key || !arr)
    {
        kt_json_free(arr);
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        kt_json_free(arr);
        return KT_ERR;
    }
    e = kt_tile_field(tile, key, 1);
    if (!e)
    {
        kt_json_free(arr);
        kt_err("out of memory");
        return KT_ERR;
    }
    text = kt_json_dump(arr);
    kt_json_free(arr);
    if (!text)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    free(e->value);
    e->value = text;
    return KT_OK;
}

static int kt_list_get_string(KtJson* arr, int index, char* buf, int cch)
{
    KtJson* v;
    const char* s = "";
    int n;
    if (!arr || index < 0 || index >= arr->count)
    {
        kt_err("index out of range");
        return KT_ERR;
    }
    v = arr->items[index];
    if (v && v->type == KT_JSON_STRING && v->text)
    {
        s = v->text;
    }
    else if (v && v->type == KT_JSON_NUMBER)
    {
        static char numbuf[32];
        sprintf(numbuf, "%lld", v->number);
        s = numbuf;
    }
    n = (int)strlen(s);
    if (buf)
    {
        if (cch <= 0 || n + 1 > cch)
        {
            kt_err("buffer too small");
            return KT_ERR;
        }
        memcpy(buf, s, (size_t)n + 1);
    }
    else if (cch != 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    return KT_OK;
}

char* kt_tile_serialize(KtTile* tile)
{
    KtJson* obj;
    KtJson* list;
    KtCfgEntry* e;
    char* text;
    if (!tile)
    {
        return NULL;
    }
    obj = kt_json_new_object();
    if (!obj)
    {
        return NULL;
    }
    kt_json_set_string(obj, "id", tile->id);
    e = tile->fields.head;
    while (e)
    {
        if (kt_streq(e->key, "peek") || kt_streq(e->key, "queue") || kt_streq(e->key, "texts"))
        {
            list = kt_json_parse(e->value ? e->value : "[]");
            if (!list || list->type != KT_JSON_ARRAY)
            {
                kt_json_free(list);
                list = kt_json_new_array();
            }
            kt_json_set(obj, e->key, list);
        }
        else if (kt_is_int_key(e->key))
        {
            kt_json_set_int(obj, e->key, e->value ? atoi(e->value) : 0);
        }
        else
        {
            kt_json_set_string(obj, e->key, e->value ? e->value : "");
        }
        e = e->next;
    }
    text = kt_json_dump(obj);
    kt_json_free(obj);
    return text;
}

KtTile* kt_tile_from_json(KtJson* obj)
{
    KtTile* tile;
    KtJsonMember* m;
    KtCfgEntry* e;
    const char* id;
    if (!obj || obj->type != KT_JSON_OBJECT)
    {
        return NULL;
    }
    id = kt_json_get_string(obj, "id", NULL);
    if (!id || !id[0])
    {
        return NULL;
    }
    tile = kt_tile_new(id);
    if (!tile)
    {
        return NULL;
    }
    m = obj->members;
    while (m)
    {
        if (kt_streq(m->key, "id"))
        {
            m = m->next;
            continue;
        }
        e = kt_tile_field(tile, m->key, 1);
        if (e)
        {
            char* text = NULL;
            if (m->value && m->value->type == KT_JSON_STRING)
            {
                text = kt_strdup(m->value->text ? m->value->text : "");
            }
            else if (m->value && (m->value->type == KT_JSON_ARRAY || m->value->type == KT_JSON_OBJECT))
            {
                text = kt_json_dump(m->value);
            }
            else if (m->value && m->value->type == KT_JSON_NUMBER)
            {
                char numbuf[32];
                sprintf(numbuf, "%lld", m->value->number);
                text = kt_strdup(numbuf);
            }
            else if (m->value && m->value->type == KT_JSON_BOOL)
            {
                text = kt_strdup(m->value->number ? "1" : "0");
            }
            if (text)
            {
                free(e->value);
                e->value = text;
            }
        }
        m = m->next;
    }
    return tile;
}

KtTile* kt_config_board_slot(KtConfig* cfg)
{
    if (!cfg)
    {
        return NULL;
    }
    if (!cfg->board)
    {
        cfg->board = kt_tile_new("board");
    }
    return cfg->board;
}

int kt_config_set_board(KtConfig* cfg, const char* key, const char* value)
{
    KtTile* board;
    KtCfgEntry* e;
    if (!cfg || !key || !key[0])
    {
        kt_err("invalid board field");
        return KT_ERR;
    }
    board = kt_config_board_slot(cfg);
    if (!board)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    e = kt_tile_field(board, key, 1);
    if (!e)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    free(e->value);
    e->value = kt_strdup(value ? value : "");
    if (!e->value)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    return KT_OK;
}

int kt_config_get_board(KtConfig* cfg, const char* key, char* buf, int cch)
{
    KtCfgEntry* e;
    if (!cfg || !cfg->board || !key)
    {
        return KT_ERR;
    }
    e = kt_tile_field(cfg->board, key, 0);
    if (!e)
    {
        return KT_ERR;
    }
    if (buf && cch > 0)
    {
        snprintf(buf, (size_t)cch, "%s", e->value ? e->value : "");
    }
    return KT_OK;
}

int kt_config_get_int_board(KtConfig* cfg, const char* key, int fallback)
{
    char buf[64];
    if (kt_config_get_board(cfg, key, buf, (int)sizeof(buf)) != KT_OK || !buf[0])
    {
        return fallback;
    }
    return atoi(buf);
}

int kt_config_set_int_board(KtConfig* cfg, const char* key, int value)
{
    char buf[32];
    snprintf(buf, (size_t)sizeof(buf), "%d", value);
    return kt_config_set_board(cfg, key, buf);
}
KtConfig* kt_config_from_json(KtJson* root)
{
    KtConfig* cfg;
    KtJson* tiles;
    int i;
    if (!root)
    {
        return NULL;
    }
    cfg = (KtConfig*)calloc(1, sizeof(KtConfig));
    if (!cfg)
    {
        return NULL;
    }
    if (root->type == KT_JSON_OBJECT && kt_json_get(root, "tiles"))
    {
        tiles = kt_json_get(root, "tiles");
    }
    else if (root->type == KT_JSON_ARRAY)
    {
        tiles = root;
    }
    else if (root->type == KT_JSON_OBJECT)
    {
        KtTile* one = kt_tile_from_json(root);
        if (one)
        {
            kt_config_push(cfg, one);
        }
        return cfg;
    }
    else
    {
        return cfg;
    }
    if (!tiles || tiles->type != KT_JSON_ARRAY)
    {
        return cfg;
    }
    for (i = 0; i < tiles->count; i++)
    {
        KtTile* tile = kt_tile_from_json(tiles->items[i]);
        if (!tile)
        {
            continue;
        }
        if (kt_config_find(cfg, tile->id))
        {
            kt_tile_free(tile);
            continue;
        }
        if (kt_config_push(cfg, tile) != KT_OK)
        {
            kt_tile_free(tile);
            break;
        }
    }
    {
        KtJson* board = kt_json_get(root, "board");
        if (board && board->type == KT_JSON_OBJECT)
        {
            KtJsonMember* m = board->members;
            while (m)
            {
                if (m->key && m->value)
                {
                    if (m->value->type == KT_JSON_STRING)
                    {
                        kt_config_set_board(cfg, m->key, m->value->text ? m->value->text : "");
                    }
                    else if (m->value->type == KT_JSON_NUMBER)
                    {
                        char numbuf[32];
                        snprintf(numbuf, (size_t)sizeof(numbuf), "%lld", m->value->number);
                        kt_config_set_board(cfg, m->key, numbuf);
                    }
                }
                m = m->next;
            }
        }
    }
    return cfg;
}

char* kt_config_serialize(KtConfig* cfg)
{
    KtJson* root;
    KtJson* tiles;
    char* text;
    int i;
    if (!cfg)
    {
        return NULL;
    }
    root = kt_json_new_object();
    if (!root)
    {
        return NULL;
    }
    kt_json_set_int(root, "version", 1);
    tiles = kt_json_new_array();
    for (i = 0; i < cfg->count; i++)
    {
        char* one = kt_tile_serialize(cfg->tiles[i]);
        KtJson* v;
        if (!one)
        {
            continue;
        }
        v = kt_json_parse(one);
        free(one);
        if (v)
        {
            kt_json_add(tiles, v);
        }
    }
    kt_json_set(root, "tiles", tiles);
    if (cfg->board)
    {
        KtJson* board = kt_json_new_object();
        KtCfgEntry* e = cfg->board->fields.head;
        while (e)
        {
            if (e->key && e->value)
            {
                kt_json_set_string(board, e->key, e->value);
            }
            e = e->next;
        }
        kt_json_set(root, "board", board);
    }
    text = kt_json_dump(root);
    kt_json_free(root);
    return text;
}

int kt_config_ensure(KtConfig* cfg, const char* id)
{
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid tile id");
        return KT_ERR;
    }
    if (kt_config_find(cfg, id))
    {
        return KT_OK;
    }
    return kt_config_add_tile(cfg, id);
}

KtConfig* kt_config_new(void)
{
    KtConfig* cfg = (KtConfig*)calloc(1, sizeof(KtConfig));
    if (!cfg)
    {
        kt_err("out of memory");
    }
    return cfg;
}

KtConfig* kt_config_load(const char* path)
{
    KtJson* root;
    KtConfig* cfg;
    if (!path || !path[0])
    {
        return kt_config_new();
    }
    if (!kt_file_exists(path))
    {
        return kt_config_new();
    }
    root = kt_json_parse_file(path);
    if (!root)
    {
        kt_err("config parse failed");
        return kt_config_new();
    }
    cfg = kt_config_from_json(root);
    kt_json_free(root);
    if (!cfg)
    {
        return kt_config_new();
    }
    return cfg;
}

int kt_config_save(KtConfig* cfg, const char* path)
{
    char* text;
    int rc;
    if (!cfg || !path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    text = kt_config_serialize(cfg);
    if (!text)
    {
        kt_err("serialize failed");
        return KT_ERR;
    }
    rc = kt_file_write_text(path, text);
    free(text);
    return rc;
}

void kt_config_free(KtConfig* cfg)
{
    int i;
    if (!cfg)
    {
        return;
    }
    for (i = 0; i < cfg->count; i++)
    {
        kt_tile_free(cfg->tiles[i]);
    }
    free(cfg->tiles);
    if (cfg->board)
    {
        kt_tile_free(cfg->board);
    }
    free(cfg);
}

int kt_config_clear(KtConfig* cfg)
{
    int i;
    if (!cfg)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    for (i = 0; i < cfg->count; i++)
    {
        kt_tile_free(cfg->tiles[i]);
    }
    free(cfg->tiles);
    cfg->tiles = NULL;
    cfg->count = 0;
    cfg->capacity = 0;
    return KT_OK;
}

int kt_config_tile_count(KtConfig* cfg)
{
    if (!cfg)
    {
        return 0;
    }
    return cfg->count;
}

int kt_config_tile_id_at(KtConfig* cfg, int index, char* buf, int cch)
{
    int n;
    if (!cfg || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    if (index < 0 || index >= cfg->count || !cfg->tiles[index])
    {
        kt_err("index out of range");
        return KT_ERR;
    }
    n = (int)strlen(cfg->tiles[index]->id);
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, cfg->tiles[index]->id, (size_t)n + 1);
    return KT_OK;
}

int kt_config_add_tile(KtConfig* cfg, const char* id)
{
    KtTile* tile;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid tile id");
        return KT_ERR;
    }
    if (kt_config_find(cfg, id))
    {
        kt_err("tile already exists");
        return KT_ERR;
    }
    tile = kt_tile_new(id);
    if (!tile)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    if (kt_config_push(cfg, tile) != KT_OK)
    {
        kt_tile_free(tile);
        return KT_ERR;
    }
    return KT_OK;
}

int kt_config_remove_tile(KtConfig* cfg, const char* id)
{
    int i;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    for (i = 0; i < cfg->count; i++)
    {
        if (cfg->tiles[i] && kt_streq(cfg->tiles[i]->id, id))
        {
            kt_config_remove_at(cfg, i);
            return KT_OK;
        }
    }
    kt_err("tile not found");
    return KT_ERR;
}

int kt_config_has_tile(KtConfig* cfg, const char* id)
{
    return kt_config_find(cfg, id) ? 1 : 0;
}

int kt_config_rename_tile(KtConfig* cfg, const char* id, const char* new_id)
{
    KtTile* tile;
    char* copy;
    if (!cfg || !id || !new_id || !new_id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_streq(id, new_id))
    {
        return KT_OK;
    }
    if (kt_config_find(cfg, new_id))
    {
        kt_err("tile already exists");
        return KT_ERR;
    }
    copy = kt_strdup(new_id);
    if (!copy)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    free(tile->id);
    tile->id = copy;
    return KT_OK;
}

int kt_config_get(KtConfig* cfg, const char* id, const char* key, char* buf, int cch)
{
    KtTile* tile;
    KtCfgEntry* e;
    int n;
    if (!cfg || !id || !key || !buf || cch <= 0)
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
    e = kt_tile_field(tile, key, 0);
    if (!e || !e->value)
    {
        kt_err("key not found");
        return KT_ERR;
    }
    n = (int)strlen(e->value);
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, e->value, (size_t)n + 1);
    return KT_OK;
}

int kt_config_set(KtConfig* cfg, const char* id, const char* key, const char* value)
{
    KtTile* tile;
    KtCfgEntry* e;
    char* copy;
    if (!cfg || !id || !key || !value)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    if (kt_config_ensure(cfg, id) != KT_OK)
    {
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    e = kt_tile_field(tile, key, 1);
    if (!e)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    copy = kt_strdup(value);
    if (!copy)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    free(e->value);
    e->value = copy;
    return KT_OK;
}

int kt_config_get_int(KtConfig* cfg, const char* id, const char* key, int fallback)
{
    KtTile* tile;
    KtCfgEntry* e;
    if (!cfg || !id || !key)
    {
        return fallback;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        return fallback;
    }
    e = kt_tile_field(tile, key, 0);
    if (!e || !e->value || !e->value[0])
    {
        return fallback;
    }
    return atoi(e->value);
}

int kt_config_set_int(KtConfig* cfg, const char* id, const char* key, int value)
{
    char text[32];
    sprintf(text, "%d", value);
    return kt_config_set(cfg, id, key, text);
}

int kt_config_list_count(KtConfig* cfg, const char* id, const char* key)
{
    KtJson* arr;
    int n;
    if (!cfg || !id || !key)
    {
        return 0;
    }
    arr = kt_list_json(cfg, id, key, 0);
    if (!arr)
    {
        return 0;
    }
    n = arr->type == KT_JSON_ARRAY ? arr->count : 0;
    kt_json_free(arr);
    return n;
}

int kt_config_list_get(KtConfig* cfg, const char* id, const char* key, int index, char* buf, int cch)
{
    KtJson* arr;
    int rc;
    if (!cfg || !id || !key)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 0);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    rc = kt_list_get_string(arr, index, buf, cch);
    kt_json_free(arr);
    return rc;
}

int kt_config_list_add(KtConfig* cfg, const char* id, const char* key, const char* value)
{
    KtJson* arr;
    int rc;
    if (!cfg || !id || !key || !value)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 1);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    kt_json_add(arr, kt_json_new_string(value));
    rc = kt_list_store(cfg, id, key, arr);
    if (rc != KT_OK)
    {
        return rc;
    }
    return kt_config_list_count(cfg, id, key) - 1;
}

int kt_config_list_insert(KtConfig* cfg, const char* id, const char* key, int index, const char* value)
{
    KtJson* arr;
    int i;
    int rc;
    if (!cfg || !id || !key || !value)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 1);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (index < 0 || index > arr->count)
    {
        kt_json_free(arr);
        kt_err("index out of range");
        return KT_ERR;
    }
    kt_json_add(arr, NULL);
    for (i = arr->count - 1; i > index; i--)
    {
        arr->items[i] = arr->items[i - 1];
    }
    arr->items[index] = kt_json_new_string(value);
    rc = kt_list_store(cfg, id, key, arr);
    if (rc != KT_OK)
    {
        return rc;
    }
    return index;
}

int kt_config_list_set(KtConfig* cfg, const char* id, const char* key, int index, const char* value)
{
    KtJson* arr;
    int rc;
    if (!cfg || !id || !key || !value)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 0);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (index < 0 || index >= arr->count)
    {
        kt_json_free(arr);
        kt_err("index out of range");
        return KT_ERR;
    }
    kt_json_free(arr->items[index]);
    arr->items[index] = kt_json_new_string(value);
    rc = kt_list_store(cfg, id, key, arr);
    return rc;
}

int kt_config_list_remove(KtConfig* cfg, const char* id, const char* key, int index)
{
    KtJson* arr;
    int i;
    int rc;
    if (!cfg || !id || !key)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 0);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (index < 0 || index >= arr->count)
    {
        kt_json_free(arr);
        kt_err("index out of range");
        return KT_ERR;
    }
    kt_json_free(arr->items[index]);
    for (i = index; i + 1 < arr->count; i++)
    {
        arr->items[i] = arr->items[i + 1];
    }
    arr->count--;
    arr->items[arr->count] = NULL;
    rc = kt_list_store(cfg, id, key, arr);
    return rc;
}

int kt_config_list_move(KtConfig* cfg, const char* id, const char* key, int from, int to)
{
    KtJson* arr;
    KtJson* item;
    int i;
    int rc;
    if (!cfg || !id || !key)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_list_json(cfg, id, key, 0);
    if (!arr)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (from < 0 || from >= arr->count || to < 0 || to >= arr->count)
    {
        kt_json_free(arr);
        kt_err("index out of range");
        return KT_ERR;
    }
    if (from == to)
    {
        kt_json_free(arr);
        return KT_OK;
    }
    item = arr->items[from];
    if (from < to)
    {
        for (i = from; i < to; i++)
        {
            arr->items[i] = arr->items[i + 1];
        }
    }
    else
    {
        for (i = from; i > to; i--)
        {
            arr->items[i] = arr->items[i - 1];
        }
    }
    arr->items[to] = item;
    rc = kt_list_store(cfg, id, key, arr);
    return rc;
}

int kt_config_list_clear(KtConfig* cfg, const char* id, const char* key)
{
    KtJson* arr;
    if (!cfg || !id || !key)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    arr = kt_json_new_array();
    if (!arr)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    return kt_list_store(cfg, id, key, arr);
}

int kt_config_export(KtConfig* cfg, const char* id, const char* path)
{
    KtTile* tile;
    char* text;
    int rc;
    if (!cfg || !id || !path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    text = kt_tile_serialize(tile);
    if (!text)
    {
        kt_err("serialize failed");
        return KT_ERR;
    }
    rc = kt_file_write_text(path, text);
    free(text);
    return rc;
}

int kt_config_export_all(KtConfig* cfg, const char* path)
{
    char* text;
    int rc;
    if (!cfg || !path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    text = kt_config_serialize(cfg);
    if (!text)
    {
        kt_err("serialize failed");
        return KT_ERR;
    }
    rc = kt_file_write_text(path, text);
    free(text);
    return rc;
}

int kt_config_import(KtConfig* cfg, const char* path, char* id_buf, int cch)
{
    KtJson* root;
    KtTile* tile = NULL;
    KtTile* existing;
    char id[64];
    int n = 1;
    int i;
    if (!cfg || !path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    root = kt_json_parse_file(path);
    if (!root)
    {
        kt_err("import parse failed");
        return KT_ERR;
    }
    if (root->type == KT_JSON_OBJECT && kt_json_get(root, "tiles"))
    {
        KtJson* tiles = kt_json_get(root, "tiles");
        if (tiles && tiles->type == KT_JSON_ARRAY && tiles->count > 0)
        {
            tile = kt_tile_from_json(tiles->items[0]);
        }
    }
    if (!tile)
    {
        tile = kt_tile_from_json(root);
    }
    if (!tile)
    {
        kt_json_free(root);
        kt_err("import has no tile");
        return KT_ERR;
    }
    kt_json_free(root);
    existing = kt_config_find(cfg, tile->id);
    if (existing)
    {
        while (1)
        {
            sprintf(id, "tile-%d", n);
            if (!kt_config_find(cfg, id))
            {
                break;
            }
            n++;
            if (n > 1000000)
            {
                kt_tile_free(tile);
                kt_err("no free tile id");
                return KT_ERR;
            }
        }
    }
    else
    {
        if ((int)strlen(tile->id) + 1 > (int)sizeof(id))
        {
            kt_tile_free(tile);
            kt_err("tile id too long");
            return KT_ERR;
        }
        memcpy(id, tile->id, strlen(tile->id) + 1);
    }
    free(tile->id);
    tile->id = kt_strdup(id);
    if (!tile->id)
    {
        kt_tile_free(tile);
        kt_err("out of memory");
        return KT_ERR;
    }
    if (kt_config_push(cfg, tile) != KT_OK)
    {
        kt_tile_free(tile);
        return KT_ERR;
    }
    if (id_buf && cch > 0)
    {
        id_buf[0] = '\0';
        i = (int)strlen(tile->id);
        if (i + 1 > cch)
        {
            kt_err("buffer too small");
            return KT_ERR;
        }
        memcpy(id_buf, tile->id, (size_t)i + 1);
    }
    return KT_OK;
}

int kt_config_copy_tile(KtConfig* cfg, const char* src_id, const char* dst_id)
{
    KtTile* src;
    KtTile* copy;
    char* text;
    if (!cfg || !src_id || !dst_id || !dst_id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    src = kt_config_find(cfg, src_id);
    if (!src)
    {
        kt_err("source tile not found");
        return KT_ERR;
    }
    text = kt_tile_serialize(src);
    if (!text)
    {
        kt_err("serialize failed");
        return KT_ERR;
    }
    {
        KtJson* obj = kt_json_parse(text);
        free(text);
        if (!obj)
        {
            kt_err("clone failed");
            return KT_ERR;
        }
        kt_json_set_string(obj, "id", dst_id);
        copy = kt_tile_from_json(obj);
        kt_json_free(obj);
    }
    if (!copy)
    {
        kt_err("clone failed");
        return KT_ERR;
    }
    if (kt_config_remove_tile(cfg, dst_id) != KT_OK && kt_config_find(cfg, dst_id))
    {
        kt_tile_free(copy);
        return KT_ERR;
    }
    if (kt_config_push(cfg, copy) != KT_OK)
    {
        kt_tile_free(copy);
        return KT_ERR;
    }
    return KT_OK;
}

int kt_tile_config_path(const char* shortcut, char* buf, int cch)
{
    size_t n;
    size_t suffix;
    if (!shortcut || !shortcut[0] || !buf || cch <= 0)
    {
        kt_err("shortcut is empty");
        return KT_ERR;
    }
    n = strlen(shortcut);
    suffix = strlen(KT_CONFIG_SUFFIX);
    if (n + suffix + 1 > (size_t)cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, shortcut, n);
    memcpy(buf + n, KT_CONFIG_SUFFIX, suffix + 1);
    return KT_OK;
}

int kt_tile_config_save_tile(KtConfig* cfg, const char* id)
{
    KtTile* tile;
    KtCfgEntry* e;
    char shortcut[KT_MAX_PATHBUF];
    char path[KT_MAX_PATHBUF];
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    e = kt_tile_field(tile, "shortcut", 0);
    if (!e || !e->value || !e->value[0])
    {
        kt_err("tile shortcut is empty");
        return KT_ERR;
    }
    if ((int)strlen(e->value) + 1 > KT_MAX_PATHBUF)
    {
        kt_err("shortcut path too long");
        return KT_ERR;
    }
    memcpy(shortcut, e->value, strlen(e->value) + 1);
    if (kt_tile_config_path(shortcut, path, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    return kt_config_export(cfg, id, path);
}

int kt_tile_config_load_tile(KtConfig* cfg, const char* id)
{
    char path[KT_MAX_PATHBUF];
    char shortcut[KT_MAX_PATHBUF];
    char private_name[KT_MAX_PATHBUF];
    KtJson* root;
    KtTile* tile;
    KtTile* target;
    int i;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    tile = kt_config_find(cfg, id);
    if (!tile)
    {
        kt_err("tile not found");
        return KT_ERR;
    }
    if (kt_config_get(cfg, id, "shortcut", shortcut, KT_MAX_PATHBUF) != KT_OK || !shortcut[0])
    {
        kt_err("tile shortcut is empty");
        return KT_ERR;
    }
    if (kt_tile_config_path(shortcut, path, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (!kt_file_exists(path))
    {
        kt_err("tile config not found");
        return KT_ERR;
    }
    if (kt_tile_private_name(shortcut, private_name, KT_MAX_PATHBUF) != KT_OK)
    {
        private_name[0] = '\0';
    }
    root = kt_json_parse_file(path);
    if (!root)
    {
        kt_err("tile config parse failed");
        return KT_ERR;
    }
    target = kt_tile_from_json(root);
    kt_json_free(root);
    if (!target)
    {
        kt_err("tile config invalid");
        return KT_ERR;
    }
    free(target->id);
    target->id = kt_strdup(id);
    for (i = 0; i < cfg->count; i++)
    {
        if (cfg->tiles[i] == tile)
        {
            kt_tile_free(tile);
            cfg->tiles[i] = target;
            return KT_OK;
        }
    }
    kt_tile_free(target);
    kt_err("tile not found");
    return KT_ERR;
}
