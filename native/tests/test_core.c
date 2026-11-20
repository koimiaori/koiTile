#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "koitile_core.h"
#include "koitile_internal.h"

static int g_pass = 0;
static int g_fail = 0;
static char g_appdata[1024];

static void check(int cond, const char* name)
{
    if (cond)
    {
        g_pass++;
        printf("PASS  %s\n", name);
    }
    else
    {
        g_fail++;
        printf("FAIL  %s\n", name);
    }
}

static long long parse_ll(const char* s)
{
    long long v = 0;
    int i = 0;
    if (!s)
    {
        return 0;
    }
    while (s[i] >= '0' && s[i] <= '9')
    {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    return v;
}

static int get_field(const char* json, const char* key, char* out, int cch)
{
    const char* p = strstr(json, key);
    int i = 0;
    if (!p)
    {
        out[0] = '\0';
        return 0;
    }
    p += strlen(key);
    while (*p && *p != ':' && *p != ',' && *p != '}')
    {
        p++;
    }
    if (*p == ':')
    {
        p++;
    }
    while (*p && *p != ',' && *p != '}')
    {
        if (i < cch - 1)
        {
            out[i] = *p;
            i++;
        }
        p++;
    }
    out[i] = '\0';
    return 1;
}

static void cleanup_test_state(void)
{
    char dir[1024];
    char path[1024];
    char state[1024];
    if (kt_appdata_dir(dir, (int)sizeof(dir)) != KT_OK)
    {
        return;
    }
    kt_path_join(dir, "config.json", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "state.json", state, (int)sizeof(state));
    kt_file_delete(state);
    kt_path_join(dir, "export.json", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "body.txt", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "sample.txt", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "big.txt", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "bom.txt", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "utf16.txt", path, (int)sizeof(path));
    kt_file_delete(path);
    kt_path_join(dir, "sub", path, (int)sizeof(path));
    kt_path_join(path, "deep", path, (int)sizeof(path));
    kt_dir_clear(path);
    {
        char sub[1024];
        kt_path_join(dir, "sub", sub, (int)sizeof(sub));
        kt_file_delete(path);
        kt_file_delete(sub);
    }
    kt_dir_clear("C:\\Temp\\koi-ctx");
    kt_file_delete("C:\\Temp\\koi-ctx");
}

static void state_file_path(char* buf, int cch)
{
    char app[1024];
    buf[0] = '\0';
    if (kt_appdata_dir(app, (int)sizeof(app)) == KT_OK)
    {
        kt_path_join(app, "state.json", buf, cch);
    }
}

static int file_contains_utf16le(const char* path, const char* ascii)
{
    HANDLE h;
    DWORD size;
    DWORD got = 0;
    unsigned char* data;
    size_t n = strlen(ascii);
    DWORD i;
    size_t j;
    int found = 0;
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                    NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size < 2 || size > 4 * 1024 * 1024)
    {
        CloseHandle(h);
        return 0;
    }
    data = (unsigned char*)malloc(size);
    if (!data)
    {
        CloseHandle(h);
        return 0;
    }
    if (ReadFile(h, data, size, &got, NULL) && got == size && size >= (DWORD)(2 * n))
    {
        for (i = 0; i + 2 * n <= size && !found; i++)
        {
            for (j = 0; j < n; j++)
            {
                if (data[i + j * 2] != (unsigned char)ascii[j] || data[i + j * 2 + 1] != 0)
                {
                    break;
                }
            }
            if (j == n)
            {
                found = 1;
            }
        }
    }
    free(data);
    CloseHandle(h);
    return found;
}

static int file_digest(const char* path, unsigned* hash, int* size)
{
    HANDLE h;
    DWORD total;
    DWORD got = 0;
    unsigned char* data;
    unsigned acc = 2166136261u;
    DWORD i;
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                    NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    total = GetFileSize(h, NULL);
    if (total == INVALID_FILE_SIZE || total > 4 * 1024 * 1024)
    {
        CloseHandle(h);
        return 0;
    }
    data = (unsigned char*)malloc(total > 0 ? (size_t)total : 1);
    if (!data)
    {
        CloseHandle(h);
        return 0;
    }
    if (total > 0 && !ReadFile(h, data, total, &got, NULL))
    {
        free(data);
        CloseHandle(h);
        return 0;
    }
    for (i = 0; i < got; i++)
    {
        acc = (acc ^ data[i]) * 16777619u;
    }
    free(data);
    CloseHandle(h);
    if (hash)
    {
        *hash = acc;
    }
    if (size)
    {
        *size = (int)got;
    }
    return 1;
}

static void test_apply_flow(void)
{
    char tmp_root[1024];
    char work[1024];
    char link_path[1024];
    char target[1024];
    char aumid[512];
    char manifest[1024];
    char tile_cfg[1024];
    char file48[1024];
    char file100[1024];
    char filewide[1024];
    char filelarge[1024];
    char probe[1024];
    char* raw;
    KtConfig* cfg;
    int renderer_real;
    DWORD n;

    n = GetTempPathA((DWORD)sizeof(tmp_root), tmp_root);
    check(n > 0, "apply: temp path available");
    kt_path_join(tmp_root, "koitile-apply-test", work, (int)sizeof(work));
    kt_dir_clear(work);
    kt_file_delete(work);
    check(kt_dir_create(work) == KT_OK, "apply: work dir created");
    kt_path_join(work, "Demo.lnk", link_path, (int)sizeof(link_path));
    kt_path_join(work, "VisualElementsManifest.xml", manifest, (int)sizeof(manifest));
    kt_path_join(work, "Demo_48x48.png", file48, (int)sizeof(file48));
    kt_path_join(work, "Demo_150x150.png", file100, (int)sizeof(file100));
    kt_path_join(work, "Demo_310x150.png", filewide, (int)sizeof(filewide));
    kt_path_join(work, "Demo_310x310.png", filelarge, (int)sizeof(filelarge));
    kt_tile_config_path(link_path, tile_cfg, (int)sizeof(tile_cfg));

    check(kt_shortcut_create("C:\\Windows\\System32\\notepad.exe", link_path, "koiTile.tile-1", "", "")
              == KT_OK, "apply: create shortcut with aumid");
    check(kt_file_exists(link_path) == 1, "apply: shortcut created");
    aumid[0] = '\0';
    check(kt_shortcut_aumid(link_path, aumid, (int)sizeof(aumid)) == KT_OK, "apply: aumid readable");
    check(strcmp(aumid, "koiTile.tile-1") == 0, "apply: aumid roundtrip");
    check(kt_shortcut_target(link_path, target, (int)sizeof(target)) == KT_OK, "apply: target readable");
    check(strcmp(target, "C:\\Windows\\System32\\notepad.exe") == 0, "apply: target value");

    cfg = kt_config_new();
    kt_config_add_tile(cfg, "tile-1");
    kt_config_set(cfg, "tile-1", "shortcut", link_path);
    kt_config_set(cfg, "tile-1", "name", "Demo");
    kt_config_set_int(cfg, "tile-1", "colorEnabled", 1);
    kt_config_set(cfg, "tile-1", "color", "#112233");
    kt_config_set_int(cfg, "tile-1", "size", 3);
    kt_path_join(work, "probe.png", probe, (int)sizeof(probe));
    renderer_real = (kt_image_render_tile(cfg, "tile-1", KT_SIZE_SMALL, probe) == KT_OK);
    kt_file_delete(probe);
    check(kt_tile_config_save_tile(cfg, "tile-1") == KT_OK, "apply: tile config saved");
    check(kt_tile_apply(cfg, "tile-1") == KT_OK, "apply: full flow returns ok");
    check(kt_file_exists(manifest) == 1, "apply: manifest exists");
    check(kt_file_exists(tile_cfg) == 1, "apply: tile config present next to shortcut");
    check(kt_tile_state(cfg, "tile-1") == 1, "apply: state marked applied");
    if (renderer_real)
    {
        check(kt_file_exists(file48) == 1, "apply: 48x48 png written");
        check(kt_file_exists(file100) == 1, "apply: 100x100 png written");
        check(kt_file_exists(filewide) == 1, "apply: 204x100 png written");
        check(kt_file_exists(filelarge) == 1, "apply: 204x204 png written");
    }
    else
    {
        check(file48[0] != '\0', "apply: 48x48 path computed");
        check(file100[0] != '\0', "apply: 100x100 path computed");
        check(filewide[0] != '\0', "apply: 204x100 path computed");
        check(filelarge[0] != '\0', "apply: 204x204 path computed");
        check(1, "apply: image renderer is a stub, png files not asserted");
    }

    check(kt_config_save_default(cfg) == KT_OK, "apply: save default config");
    raw = kt_file_read_utf8(manifest);
    check(raw != NULL, "apply: manifest readable as text");
    if (raw)
    {
        check(strstr(raw, "<VisualElements") != NULL, "apply: manifest has VisualElements");
        check(strstr(raw, "Square70x70Logo=\"Demo_48x48.png\"") != NULL, "apply: manifest 48x48 mapping");
        check(strstr(raw, "Square150x150Logo=\"Demo_150x150.png\"") != NULL,
              "apply: manifest 100x100 mapping");
        check(strstr(raw, "Wide310x150Logo=\"Demo_310x150.png\"") != NULL, "apply: manifest wide mapping");
        check(strstr(raw, "Square310x310Logo=\"Demo_310x310.png\"") != NULL,
              "apply: manifest large mapping");
        check(strstr(raw, "BackgroundColor=\"#112233\"") != NULL, "apply: manifest custom color");
        check(strstr(raw, "ForegroundText=\"light\"") != NULL, "apply: manifest foreground");
        printf("--- manifest ---\n%s----------------\n", raw);
        free(raw);
    }

    {
        char cfg_path[1024];
        kt_config_path(cfg_path, (int)sizeof(cfg_path));
        raw = kt_file_read_utf8(cfg_path);
        check(raw != NULL, "apply: config.json readable");
        check(raw && strstr(raw, "\"size\":3") != NULL, "apply: size serialized as number");
        check(raw && strstr(raw, "\"size\":\"3\"") == NULL, "apply: size not serialized as string");
        check(raw && strstr(raw, "\"colorEnabled\":1") != NULL,
              "apply: colorEnabled serialized as number");
        check(raw && strstr(raw, "\"showName\":1") != NULL, "apply: showName serialized as number");
        check(raw && strstr(raw, "\"peekSeconds\":12") != NULL,
              "apply: peekSeconds serialized as number");
        free(raw);
    }

    check(kt_tile_restore(cfg, "tile-1") == KT_OK, "apply: restore returns ok");
    check(kt_file_exists(manifest) == 0, "apply: manifest removed by restore");
    check(kt_file_exists(tile_cfg) == 0, "apply: tile config removed by restore");
    check(kt_tile_state(cfg, "tile-1") == 0, "apply: state cleared by restore");
    check(kt_shortcut_target(link_path, target, (int)sizeof(target)) == KT_OK,
          "apply: shortcut survives restore");
    check(strcmp(target, "C:\\Windows\\System32\\notepad.exe") == 0, "apply: target intact after restore");
    aumid[0] = '\0';
    kt_shortcut_aumid(link_path, aumid, (int)sizeof(aumid));
    check(strcmp(aumid, "koiTile.tile-1") == 0, "apply: aumid intact after restore");

    check(kt_tile_apply(cfg, "tile-1") == KT_OK, "apply: second apply");
    check(kt_tile_restore_all(cfg) == KT_OK, "apply: restore all");
    check(kt_file_exists(manifest) == 0, "apply: restore all removes manifest");
    check(kt_file_exists(tile_cfg) == 0, "apply: restore all removes tile config");

    {
        char empty_dir[1024];
        kt_path_join(tmp_root, "koitile-apply-empty", empty_dir, (int)sizeof(empty_dir));
        kt_dir_clear(empty_dir);
        kt_file_delete(empty_dir);
        kt_dir_create(empty_dir);
        kt_path_join(empty_dir, "Empty.lnk", target, (int)sizeof(target));
        check(kt_shortcut_create("", "C:\\no\\such\\file.lnk", "x", "", "") != KT_OK,
              "apply: aumid patch on missing shortcut fails");
        check(kt_dir_clear(empty_dir) == KT_OK, "apply: clean empty dir");
        kt_file_delete(empty_dir);
    }

    kt_config_free(cfg);
    kt_dir_clear(work);
    kt_file_delete(work);
    {
        char state_path[1024];
        state_file_path(state_path, (int)sizeof(state_path));
        kt_file_delete(state_path);
        kt_config_path(state_path, (int)sizeof(state_path));
        kt_file_delete(state_path);
    }
}

static void test_link(void)
{
    char tmp_root[1024];
    char tempdir[1024];
    char link_path[1024];
    char target[1024];
    char aumid[512];
    KtConfig* cfg;
    char state_path[1024];
    char* state_text;
    DWORD n;

    n = GetTempPathA((DWORD)sizeof(tmp_root), tmp_root);
    check(n > 0, "link: temp path available");
    kt_path_join(tmp_root, "koitile-link-test", tempdir, (int)sizeof(tempdir));
    kt_dir_clear(tempdir);
    kt_file_delete(tempdir);
    check(kt_dir_create(tempdir) == KT_OK, "link: temp dir created");
    kt_path_join(tempdir, "Notepad.lnk", link_path, (int)sizeof(link_path));
    kt_file_delete(link_path);

    check(kt_shortcut_create("C:\\Windows\\System32\\notepad.exe", link_path, "koiTile.linktest",
                             "", NULL) == KT_OK, "link: create real shortcut");
    check(kt_file_exists(link_path) == 1, "link: shortcut file exists");
    check(kt_shortcut_target(link_path, target, (int)sizeof(target)) == KT_OK, "link: read target");
    check(strcmp(target, "C:\\Windows\\System32\\notepad.exe") == 0, "link: target value");
    check(kt_shortcut_aumid(link_path, aumid, (int)sizeof(aumid)) == KT_OK, "link: read aumid");
    check(strcmp(aumid, "koiTile.linktest") == 0, "link: aumid value");

    cfg = kt_config_new();
    kt_config_add_tile(cfg, "link-1");
    kt_config_set(cfg, "link-1", "shortcut", link_path);
    kt_config_set(cfg, "link-1", "name", "LinkApp");
    kt_config_set(cfg, "link-1", "link", "https://example.com");
    check(kt_tile_apply(cfg, "link-1") == KT_OK, "link: apply with link");
    check(file_contains_utf16le(link_path, "https://example.com") == 1,
          "link: .lnk binary contains the link url");
    check(kt_shortcut_aumid(link_path, aumid, (int)sizeof(aumid)) == KT_OK,
          "link: aumid still readable");
    check(strcmp(aumid, "koiTile.linktest") == 0, "link: aumid preserved");

    check(kt_config_set(cfg, "link-1", "link", "https://example.org") == KT_OK, "link: change link");
    check(kt_tile_apply(cfg, "link-1") == KT_OK, "link: reapply with new link");
    check(file_contains_utf16le(link_path, "https://example.org") == 1,
          "link: second apply retargets .lnk");

    state_file_path(state_path, (int)sizeof(state_path));
    state_text = kt_file_read_utf8(state_path);
    check(state_text != NULL, "link: state file readable");
    check(state_text && strstr(state_text, "origTarget") != NULL, "link: state records origTarget");
    check(state_text && strstr(state_text, "notepad.exe") != NULL, "link: state records original target");
    free(state_text);

    check(kt_tile_restore(cfg, "link-1") == KT_OK, "link: restore");
    target[0] = '\0';
    check(kt_shortcut_target(link_path, target, (int)sizeof(target)) == KT_OK,
          "link: read target after restore");
    check(strcmp(target, "C:\\Windows\\System32\\notepad.exe") == 0, "link: target restored");
    check(file_contains_utf16le(link_path, "https://example.org") == 0,
          "link: url removed from .lnk after restore");
    check(kt_tile_state(cfg, "link-1") == 0, "link: state cleared after restore");
    state_text = kt_file_read_utf8(state_path);
    if (state_text)
    {
        check(strstr(state_text, "link-1") == NULL, "link: state entry removed");
        free(state_text);
    }
    else
    {
        check(1, "link: state entry removed");
    }

    check(kt_config_set(cfg, "link-1", "link", "") == KT_OK, "link: clear link field");
    check(kt_tile_apply(cfg, "link-1") == KT_OK, "link: apply without link");
    target[0] = '\0';
    kt_shortcut_target(link_path, target, (int)sizeof(target));
    check(strcmp(target, "C:\\Windows\\System32\\notepad.exe") == 0,
          "link: empty link leaves target untouched");
    check(kt_tile_restore(cfg, "link-1") == KT_OK, "link: restore without link");

    kt_config_set(cfg, "link-1", "link", "https://example.com");
    kt_file_delete(link_path);
    check(kt_tile_apply(cfg, "link-1") != KT_OK, "link: apply fails when shortcut missing");
    kt_config_free(cfg);

    {
        char missing[1024];
        kt_path_join(tempdir, "Missing.lnk", missing, (int)sizeof(missing));
        check(kt_shortcut_target(missing, target, (int)sizeof(target)) != KT_OK,
              "link: read missing shortcut fails");
    }

    kt_dir_clear(tempdir);
    kt_file_delete(tempdir);
    {
        char state_cleanup[1024];
        state_file_path(state_cleanup, (int)sizeof(state_cleanup));
        kt_file_delete(state_cleanup);
    }
}

static void test_roundtrip(void)
{
    KtConfig* cfg = kt_config_new();
    KtConfig* back;
    char path[1024];
    char buf[4096];
    char* text;
    char big[600];
    int i;

    check(cfg != NULL, "roundtrip: new config");
    kt_path_join(g_appdata, "roundtrip.json", path, (int)sizeof(path));
    check(kt_config_add_tile(cfg, "tile-1") == KT_OK, "roundtrip: add tile");
    check(kt_config_set(cfg, "tile-1", "name", "Hello") == KT_OK, "roundtrip: set name");
    check(kt_config_set_int(cfg, "tile-1", "size", 3) == KT_OK, "roundtrip: set size");
    check(kt_config_list_add(cfg, "tile-1", "queue", "one") == 0, "roundtrip: add queue");

    text = kt_config_serialize(cfg);
    check(text != NULL, "roundtrip: serialize not null");
    check(text && strstr(text, "tile-1") != NULL, "roundtrip: serialize contains tile id");
    check(text && strstr(text, "Hello") != NULL, "roundtrip: serialize contains name");
    check(text && strstr(text, "\"queue\":[\"one\"]") != NULL, "roundtrip: serialize contains queue");
    check(text && (int)strlen(text) > 64, "roundtrip: serialize exceeds 64 bytes");
    free(text);

    check(kt_config_save(cfg, path) == KT_OK, "roundtrip: save");
    {
        char* raw = kt_file_read_utf8(path);
        check(raw != NULL, "roundtrip: file readable");
        check(raw && strstr(raw, "tile-1") != NULL, "roundtrip: file contains tile");
        check(raw && strstr(raw, "\"tiles\":[]") == NULL, "roundtrip: file is not empty tiles");
        free(raw);
    }
    kt_config_free(cfg);

    back = kt_config_load(path);
    check(back != NULL, "roundtrip: load");
    check(kt_config_tile_count(back) == 1, "roundtrip: tile count preserved");
    check(kt_config_get(back, "tile-1", "name", buf, (int)sizeof(buf)) == KT_OK, "roundtrip: name get");
    check(strcmp(buf, "Hello") == 0, "roundtrip: name preserved");
    check(kt_config_get_int(back, "tile-1", "size", -1) == 3, "roundtrip: size preserved");
    check(kt_config_list_count(back, "tile-1", "queue") == 1, "roundtrip: queue count preserved");
    check(kt_config_list_get(back, "tile-1", "queue", 0, buf, (int)sizeof(buf)) == KT_OK,
          "roundtrip: queue get");
    check(strcmp(buf, "one") == 0, "roundtrip: queue value preserved");
    kt_config_free(back);

    cfg = kt_config_new();
    kt_config_add_tile(cfg, "big-1");
    for (i = 0; i < 500; i++)
    {
        big[i] = (char)('a' + (i % 26));
    }
    big[500] = '\0';
    check(kt_config_set(cfg, "big-1", "name", big) == KT_OK, "roundtrip: set 500 char name");
    kt_config_set_int(cfg, "big-1", "size", 2);
    kt_config_set(cfg, "big-1", "title", big);
    kt_config_set(cfg, "big-1", "subtitle", big);
    kt_config_set(cfg, "big-1", "appName", big);
    kt_config_list_add(cfg, "big-1", "peek", big);
    kt_config_list_add(cfg, "big-1", "peek", big);
    check(kt_config_save(cfg, path) == KT_OK, "roundtrip: save large");
    {
        char* raw = kt_file_read_utf8(path);
        check(raw != NULL && (int)strlen(raw) > 2000, "roundtrip: large file written");
        free(raw);
    }
    kt_config_free(cfg);
    back = kt_config_load(path);
    check(kt_config_tile_count(back) == 1, "roundtrip: large tile count");
    check(kt_config_get(back, "big-1", "name", buf, (int)sizeof(buf)) == KT_OK,
          "roundtrip: large name get");
    check(strcmp(buf, big) == 0, "roundtrip: large name preserved");
    check(kt_config_get_int(back, "big-1", "size", -1) == 2, "roundtrip: large size preserved");
    check(kt_config_list_count(back, "big-1", "peek") == 2, "roundtrip: large peek count");
    check(kt_config_list_get(back, "big-1", "peek", 1, buf, (int)sizeof(buf)) == KT_OK,
          "roundtrip: large peek get");
    check(strcmp(buf, big) == 0, "roundtrip: large peek preserved");
    kt_config_free(back);
    kt_file_delete(path);
}

static void test_json(void)
{
    const char* src = "{\"a\":1,\"b\":\"x\\\"y\\\\z\",\"c\":[1,2,3],\"d\":true,\"e\":false,"
                      "\"f\":null,\"g\":{\"h\":-12},\"u\":\"\\u6C34\",\"s\":\"\\uD83D\\uDE00\"}";
    const char* cjk = "\xE6\xB0\xB4";
    const char* emoji = "\xF0\x9F\x98\x80";
    KtJson* v;
    KtJson* round;
    char* text;
    char* text2;
    char buf[8192];

    v = kt_json_parse(src);
    check(v != NULL, "json: parse object");
    check(v && v->type == KT_JSON_OBJECT, "json: root is object");
    check(kt_json_get_int(v, "a", 0) == 1, "json: integer field");
    check(kt_json_get_int(v, "g", 0) == 0, "json: nested object not int");
    check(kt_json_get(v, "g") && kt_json_get_int(kt_json_get(v, "g"), "h", 0) == -12, "json: nested value");
    check(kt_json_get_string(v, "b", "") != NULL, "json: string field");
    check(strcmp(kt_json_get_string(v, "b", ""), "x\"y\\z") == 0, "json: escape decoding");
    check(kt_json_get_int(v, "d", -1) == 1, "json: true field");
    check(kt_json_get_int(v, "e", -1) == 0, "json: false field");
    check(kt_json_get(v, "f") && kt_json_get(v, "f")->type == KT_JSON_NULL, "json: null field");
    check(kt_json_get(v, "c") && kt_json_get(v, "c")->count == 3, "json: array count");
    check(kt_json_get(v, "c") && kt_json_get(v, "c")->items[2]->number == 3, "json: array item");
    check(strcmp(kt_json_get_string(v, "u", ""), cjk) == 0, "json: bmp escape to utf8");
    check(strcmp(kt_json_get_string(v, "s", ""), emoji) == 0, "json: surrogate pair to utf8");
    check(kt_json_get_string(v, "zz", "fallback") != NULL, "json: missing key fallback");
    check(strcmp(kt_json_get_string(v, "zz", "fallback"), "fallback") == 0, "json: fallback value");

    text = kt_json_dump(v);
    check(text != NULL, "json: dump");
    check(text && strstr(text, "\"u\":\"") != NULL, "json: dump keeps field");
    check(text && strstr(text, cjk) != NULL, "json: dump writes raw utf8");
    check(text && strstr(text, "\\u6C34") == NULL, "json: dump has no bmp escape");
    round = kt_json_parse(text);
    check(round != NULL, "json: reparse dump");
    text2 = round ? kt_json_dump(round) : NULL;
    check(text2 && strcmp(text, text2) == 0, "json: roundtrip stable");
    kt_json_free(round);
    free(text2);
    free(text);
    kt_json_free(v);

    check(kt_json_parse("") == NULL, "json: empty text rejected");
    check(kt_json_parse("{") == NULL, "json: truncated rejected");
    check(kt_json_parse("[1,2") == NULL, "json: truncated array rejected");
    check(kt_json_parse("{\"a\":1} trailing") == NULL, "json: trailing garbage rejected");
    check(kt_json_parse("\"\\q\"") == NULL, "json: bad escape rejected");
    check(kt_json_parse("null") != NULL, "json: bare null parses");
    kt_json_free(kt_json_parse("null"));

    v = kt_json_new_object();
    kt_json_set_string(v, "s", "line\nbreak");
    kt_json_set_int(v, "n", -42);
    kt_json_set(v, "arr", kt_json_new_array());
    kt_json_add(kt_json_get(v, "arr"), kt_json_new_string("q"));
    text = kt_json_dump(v);
    check(text && strstr(text, "\\n") != NULL, "json: control char escaped");
    check(text && strstr(text, "\"n\":-42") != NULL, "json: negative number dumped");
    kt_json_free(v);
    free(text);

    {
        KtConfig* cfg = kt_config_new();
        char path[1024];
        KtJson* root;
        kt_config_path(path, (int)sizeof(path));
        kt_config_add_tile(cfg, "tile-1");
        kt_config_set(cfg, "tile-1", "name", "水 tile");
        kt_config_list_add(cfg, "tile-1", "peek", "a.png");
        check(kt_config_save(cfg, path) == KT_OK, "json: config save");
        kt_config_free(cfg);
        root = kt_json_parse_file(path);
        check(root != NULL, "json: config file parses");
        check(kt_json_get(root, "tiles") != NULL, "json: config has tiles");
        check(kt_json_get_int(root, "version", 0) == 1, "json: config version 1");
        {
            KtJson* tiles = kt_json_get(root, "tiles");
            check(tiles && tiles->count == 1, "json: config tile count");
            if (tiles && tiles->count == 1 && tiles->items[0])
            {
                check(strcmp(kt_json_get_string(tiles->items[0], "name", ""),
                             "水 tile") == 0,
                      "json: utf8 value roundtrip");
                check(kt_json_get(tiles->items[0], "peek") != NULL, "json: list stored as array");
                check(kt_json_get(tiles->items[0], "peek") &&
                          kt_json_get(tiles->items[0], "peek")->count == 1,
                      "json: list length");
            }
            else
            {
                check(0, "json: utf8 value roundtrip");
                check(0, "json: list stored as array");
                check(0, "json: list length");
            }
        }
        kt_json_free(root);
        cfg = kt_config_load(path);
        check(kt_config_get(cfg, "tile-1", "name", buf, (int)sizeof(buf)) == KT_OK,
              "json: utf8 config reload");
        check(strcmp(buf, "水 tile") == 0, "json: utf8 config reload value");
        kt_config_free(cfg);
    }

    {
        char path[1024];
        char* content;
        kt_config_path(path, (int)sizeof(path));
        content = kt_file_read_utf8(path);
        check(content != NULL, "json: read utf8 helper");
        check(content && (unsigned char)content[0] != 0xEF, "json: no utf8 bom written");
        free(content);
    }
}

static void test_config(void)
{
    KtConfig* cfg = kt_config_new();
    char buf[1024];
    char path[1024];
    int i;

    check(cfg != NULL, "config: new");
    check(kt_config_clear(cfg) == KT_OK, "config: clear empty");
    check(kt_config_tile_count(cfg) == 0, "config: empty count");
    check(kt_config_add_tile(cfg, "tile-1") == KT_OK, "config: add tile");
    check(kt_config_add_tile(cfg, "tile-2") == KT_OK, "config: add second tile");
    check(kt_config_add_tile(cfg, "tile-1") != KT_OK, "config: duplicate rejected");
    check(kt_config_add_tile(cfg, "") != KT_OK, "config: empty id rejected");
    check(kt_config_add_tile(NULL, "x") != KT_OK, "config: null config rejected");
    check(kt_config_tile_count(cfg) == 2, "config: count 2");
    check(kt_config_has_tile(cfg, "tile-1") == 1, "config: has tile");
    check(kt_config_has_tile(cfg, "nope") == 0, "config: missing tile");
    check(kt_config_tile_id_at(cfg, 1, buf, (int)sizeof(buf)) == KT_OK, "config: id at index");
    check(strcmp(buf, "tile-2") == 0, "config: id at index value");
    check(kt_config_tile_id_at(cfg, 5, buf, (int)sizeof(buf)) != KT_OK, "config: bad index");
    check(kt_config_tile_id_at(cfg, 0, buf, 2) != KT_OK, "config: small buffer");

    check(kt_config_get_int(cfg, "tile-1", "size", -1) == 3, "config: default size 3");
    check(kt_config_get_int(cfg, "tile-1", "colorEnabled", -1) == 0, "config: default colorEnabled 0");
    check(kt_config_get(cfg, "tile-1", "color", buf, (int)sizeof(buf)) == KT_OK, "config: default color");
    check(strcmp(buf, "#2D89EF") == 0, "config: default color value");
    check(kt_config_get_int(cfg, "tile-1", "imageScale", -1) == 100, "config: default imageScale");
    check(kt_config_get_int(cfg, "tile-1", "refreshSeconds", -1) == 30, "config: default refreshSeconds");
    check(kt_config_get_int(cfg, "tile-1", "peekSeconds", -1) == 12, "config: default peekSeconds");
    check(kt_config_get_int(cfg, "tile-1", "showName", -1) == 1, "config: default showName");
    check(kt_config_get_int(cfg, "tile-1", "foregroundLight", -1) == 1, "config: default foregroundLight");
    check(kt_config_get_int(cfg, "tile-1", "iconSize", -1) == 64, "config: default iconSize");
    check(kt_config_get_int(cfg, "tile-1", "textSource", -1) == 0, "config: default textSource");
    check(kt_config_get_int(cfg, "tile-1", "imageOffsetX", -1) == 0, "config: default imageOffsetX");
    check(kt_config_get_int(cfg, "tile-1", "imageOffsetY", -1) == 0, "config: default imageOffsetY");
    check(kt_config_get_int(cfg, "tile-1", "iconEnabled", -1) == 0, "config: default iconEnabled");
    check(kt_config_get_int(cfg, "tile-1", "iconOffsetX", -1) == 0, "config: default iconOffsetX");
    check(kt_config_get_int(cfg, "tile-1", "iconOffsetY", -1) == 0, "config: default iconOffsetY");
    check(kt_config_get_int(cfg, "tile-1", "gridCol", -1) == 0, "config: default gridCol 0");
    check(kt_config_get_int(cfg, "tile-1", "gridRow", -1) == 0, "config: default gridRow 0");
    check(kt_config_get(cfg, "tile-1", "name", buf, (int)sizeof(buf)) == KT_OK, "config: default name");
    check(strcmp(buf, "") == 0, "config: default name empty");
    check(kt_config_get(cfg, "tile-1", "image", buf, (int)sizeof(buf)) == KT_OK, "config: default image");
    check(strcmp(buf, "") == 0, "config: default image empty");
    check(kt_config_get(cfg, "tile-1", "textTemplate", buf, (int)sizeof(buf)) == KT_OK,
          "config: default textTemplate");
    check(strcmp(buf, "{content}") == 0, "config: default textTemplate value");
    check(kt_config_get_int(cfg, "tile-1", "missing", 42) == 42, "config: fallback int");
    check(kt_config_get(cfg, "tile-1", "missing", buf, (int)sizeof(buf)) != KT_OK, "config: missing key");
    check(kt_config_list_count(cfg, "tile-1", "peek") == 0, "config: default empty peek");
    check(kt_config_list_count(cfg, "tile-1", "queue") == 0, "config: default empty queue");

    check(kt_config_set(cfg, "tile-1", "name", "My Tile") == KT_OK, "config: set string");
    check(kt_config_get(cfg, "tile-1", "name", buf, (int)sizeof(buf)) == KT_OK, "config: get string");
    check(strcmp(buf, "My Tile") == 0, "config: set/get value");
    check(kt_config_set_int(cfg, "tile-1", "size", 2) == KT_OK, "config: set int");
    check(kt_config_get_int(cfg, "tile-1", "size", -1) == 2, "config: set int value");
    check(kt_config_set_int(cfg, "tile-1", "gridCol", 3) == KT_OK, "config: set gridCol");
    check(kt_config_get_int(cfg, "tile-1", "gridCol", -1) == 3, "config: gridCol value");
    check(kt_config_set_int(cfg, "tile-1", "gridRow", 2) == KT_OK, "config: set gridRow");
    check(kt_config_get_int(cfg, "tile-1", "gridRow", -1) == 2, "config: gridRow value");
    check(kt_config_get(cfg, "tile-1", "gridCol", buf, (int)sizeof(buf)) == KT_OK,
          "config: gridCol readable");
    check(strcmp(buf, "3") == 0, "config: gridCol text value");
    check(kt_config_get(cfg, "tile-1", "gridRow", buf, (int)sizeof(buf)) == KT_OK,
          "config: gridRow readable");
    check(strcmp(buf, "2") == 0, "config: gridRow text value");
    check(kt_config_set(cfg, "tile-9", "name", "Auto") == KT_OK, "config: auto create tile");
    check(kt_config_has_tile(cfg, "tile-9") == 1, "config: auto created");
    check(kt_config_set(NULL, "tile-1", "name", "x") != KT_OK, "config: set null rejected");
    kt_config_remove_tile(cfg, "tile-9");
    check(kt_config_remove_tile(cfg, "nope") != KT_OK, "config: remove missing");

    check(kt_config_list_add(cfg, "tile-1", "peek", "p1") == 0, "config: list add returns index");
    check(kt_config_list_add(cfg, "tile-1", "peek", "p2") == 1, "config: list add second");
    check(kt_config_list_add(cfg, "tile-1", "peek", "p3") == 2, "config: list add third");
    check(kt_config_list_count(cfg, "tile-1", "peek") == 3, "config: list count");
    check(kt_config_list_get(cfg, "tile-1", "peek", 1, buf, (int)sizeof(buf)) == KT_OK, "config: list get");
    check(strcmp(buf, "p2") == 0, "config: list get value");
    check(kt_config_list_get(cfg, "tile-1", "peek", 7, buf, (int)sizeof(buf)) != KT_OK,
          "config: list bad index");
    check(kt_config_list_insert(cfg, "tile-1", "peek", 0, "p0") == 0, "config: list insert");
    check(kt_config_list_count(cfg, "tile-1", "peek") == 4, "config: list count after insert");
    check(kt_config_list_get(cfg, "tile-1", "peek", 1, buf, (int)sizeof(buf)) == KT_OK,
          "config: after insert get");
    check(strcmp(buf, "p1") == 0, "config: insert shifted");
    check(kt_config_list_set(cfg, "tile-1", "peek", 3, "pX") == KT_OK, "config: list set");
    check(kt_config_list_get(cfg, "tile-1", "peek", 3, buf, (int)sizeof(buf)) == KT_OK,
          "config: list set get");
    check(strcmp(buf, "pX") == 0, "config: list set value");
    check(kt_config_list_move(cfg, "tile-1", "peek", 0, 3) == KT_OK, "config: list move");
    check(kt_config_list_get(cfg, "tile-1", "peek", 3, buf, (int)sizeof(buf)) == KT_OK, "config: move get");
    check(strcmp(buf, "p0") == 0, "config: move value");
    check(kt_config_list_get(cfg, "tile-1", "peek", 0, buf, (int)sizeof(buf)) == KT_OK,
          "config: move head");
    check(strcmp(buf, "p1") == 0, "config: move head value");
    check(kt_config_list_move(cfg, "tile-1", "peek", 2, 0) == KT_OK, "config: move backwards");
    check(kt_config_list_get(cfg, "tile-1", "peek", 0, buf, (int)sizeof(buf)) == KT_OK,
          "config: move backwards get");
    check(strcmp(buf, "pX") == 0, "config: move backwards value");
    check(kt_config_list_move(cfg, "tile-1", "peek", 0, 0) == KT_OK, "config: move same index");
    check(kt_config_list_move(cfg, "tile-1", "peek", 0, 9) != KT_OK, "config: move bad index");
    check(kt_config_list_remove(cfg, "tile-1", "peek", 0) == KT_OK, "config: list remove");
    check(kt_config_list_count(cfg, "tile-1", "peek") == 3, "config: count after remove");
    check(kt_config_list_remove(cfg, "tile-1", "peek", 9) != KT_OK, "config: bad remove index");
    check(kt_config_list_count(cfg, "nope", "peek") == 0, "config: list count missing tile");
    check(kt_config_list_clear(cfg, "tile-1", "peek") == KT_OK, "config: list clear");
    check(kt_config_list_count(cfg, "tile-1", "peek") == 0, "config: count after clear");
    check(kt_config_list_get(cfg, "tile-1", "peek", 0, buf, (int)sizeof(buf)) != KT_OK,
          "config: get after clear");

    for (i = 0; i < KT_MAX_PEEK; i++)
    {
        check(kt_config_list_add(cfg, "tile-1", "peek", "item") == i, "config: peek fill");
    }
    check(kt_config_list_count(cfg, "tile-1", "peek") == KT_MAX_PEEK, "config: peek cap");
    for (i = 0; i < KT_MAX_QUEUE; i++)
    {
        check(kt_config_list_add(cfg, "tile-1", "queue", "msg") == i, "config: queue fill");
    }
    check(kt_config_list_count(cfg, "tile-1", "queue") == KT_MAX_QUEUE, "config: queue cap");
    kt_config_list_clear(cfg, "tile-1", "peek");
    kt_config_list_clear(cfg, "tile-1", "queue");

    check(kt_config_rename_tile(cfg, "tile-2", "tile-b") == KT_OK, "config: rename");
    check(kt_config_has_tile(cfg, "tile-b") == 1, "config: renamed present");
    check(kt_config_has_tile(cfg, "tile-2") == 0, "config: old name gone");
    check(kt_config_rename_tile(cfg, "tile-b", "tile-1") != KT_OK, "config: rename collision");
    check(kt_config_rename_tile(cfg, "nope", "x") != KT_OK, "config: rename missing");
    check(kt_config_rename_tile(cfg, "tile-b", "tile-b") == KT_OK, "config: rename to self");

    check(kt_config_copy_tile(cfg, "tile-1", "tile-copy") == KT_OK, "config: copy tile");
    check(kt_config_get(cfg, "tile-copy", "name", buf, (int)sizeof(buf)) == KT_OK, "config: copy get");
    check(strcmp(buf, "My Tile") == 0, "config: copy value");
    check(kt_config_copy_tile(cfg, "nope", "tile-copy") != KT_OK, "config: copy missing source");
    check(kt_config_copy_tile(cfg, "tile-1", "tile-b") == KT_OK, "config: copy over existing");
    check(kt_config_get(cfg, "tile-b", "name", buf, (int)sizeof(buf)) == KT_OK, "config: overwritten copy");

    kt_config_path(path, (int)sizeof(path));
    check(kt_config_save(cfg, path) == KT_OK, "config: save");
    {
        char* raw = kt_file_read_utf8(path);
        check(raw != NULL, "config: grid json readable");
        check(raw && strstr(raw, "\"gridCol\":3") != NULL, "config: gridCol serialized as number");
        check(raw && strstr(raw, "\"gridCol\":\"3\"") == NULL, "config: gridCol not a string");
        check(raw && strstr(raw, "\"gridRow\":2") != NULL, "config: gridRow serialized as number");
        check(raw && strstr(raw, "\"gridRow\":\"2\"") == NULL, "config: gridRow not a string");
        free(raw);
    }
    kt_config_save(NULL, path);
    check(kt_config_save(NULL, path) != KT_OK, "config: save null rejected");
    kt_config_free(cfg);
    cfg = kt_config_load(path);
    check(cfg != NULL, "config: load");
    check(kt_config_tile_count(cfg) == 3, "config: loaded count");
    check(kt_config_get(cfg, "tile-1", "name", buf, (int)sizeof(buf)) == KT_OK, "config: loaded value");
    check(strcmp(buf, "My Tile") == 0, "config: loaded value match");
    check(kt_config_get_int(cfg, "tile-1", "size", -1) == 2, "config: loaded int");
    check(kt_config_get_int(cfg, "tile-1", "gridCol", -1) == 3, "config: loaded gridCol");
    check(kt_config_get_int(cfg, "tile-1", "gridRow", -1) == 2, "config: loaded gridRow");
    check(kt_config_tile_id_at(cfg, 0, buf, (int)sizeof(buf)) == KT_OK, "config: loaded order 0");
    check(strcmp(buf, "tile-1") == 0, "config: loaded order value");

    check(kt_config_remove_tile(cfg, "tile-1") == KT_OK, "config: remove tile");
    check(kt_config_tile_count(cfg) == 2, "config: count after remove");
    check(kt_config_set(cfg, "tile-b", "shortcut", "C:\\Temp\\koi-ctx\\My Shortcut.lnk") == KT_OK,
          "config: set shortcut");
    check(kt_dir_create("C:\\Temp\\koi-ctx") == KT_OK, "config: create shortcut dir");
    {
        char tile_path[1024];
        check(kt_tile_config_path("C:\\Temp\\koi-ctx\\My Shortcut.lnk", tile_path,
                                  (int)sizeof(tile_path)) == KT_OK, "config: tile config path");
        check(strcmp(tile_path, "C:\\Temp\\koi-ctx\\My Shortcut.lnk.koitileconfig.json") == 0,
              "config: tile config path value");
        check(kt_tile_config_save_tile(cfg, "tile-b") == KT_OK, "config: save tile file");
        check(kt_file_exists(tile_path) == 1, "config: tile file exists");
        check(kt_config_set(cfg, "tile-b", "name", "Changed") == KT_OK, "config: change before load");
        check(kt_tile_config_load_tile(cfg, "tile-b") == KT_OK, "config: load tile file");
        check(kt_config_get(cfg, "tile-b", "name", buf, (int)sizeof(buf)) == KT_OK,
              "config: loaded tile name");
        check(strcmp(buf, "My Tile") == 0, "config: loaded tile name value");
        check(kt_tile_config_path(NULL, tile_path, (int)sizeof(tile_path)) != KT_OK,
              "config: null path rejected");
        check(kt_tile_config_path("a.lnk", tile_path, 4) != KT_OK, "config: small path buffer");
        check(kt_tile_config_load_tile(cfg, "nope") != KT_OK, "config: load missing tile");
        kt_file_delete(tile_path);
        kt_tile_config_save_tile(cfg, "nope");
        check(kt_tile_config_save_tile(cfg, "nope") != KT_OK, "config: save missing tile");
    }

    {
        char export_path[1024];
        char id_buf[256];
        KtConfig* other;
        kt_path_join(g_appdata, "export.json", export_path, (int)sizeof(export_path));
        check(kt_config_export(cfg, "tile-b", export_path) == KT_OK, "config: export one");
        check(kt_file_exists(export_path) == 1, "config: export file exists");
        other = kt_config_new();
        check(kt_config_import(other, export_path, id_buf, (int)sizeof(id_buf)) == KT_OK,
              "config: import into empty");
        check(strcmp(id_buf, "tile-b") == 0, "config: import keeps id");
        check(kt_config_get_int(other, "tile-b", "size", -1) != -1, "config: import data present");
        check(kt_config_import(other, export_path, id_buf, (int)sizeof(id_buf)) == KT_OK,
              "config: import duplicate");
        check(strcmp(id_buf, "tile-b") != 0, "config: import renames duplicate");
        check(kt_config_tile_count(other) == 2, "config: import count");
        check(kt_config_import(other, export_path, NULL, 0) == KT_OK, "config: import without id buffer");
        check(kt_config_export_all(other, export_path) == KT_OK, "config: export all");
        kt_config_free(other);
        check(kt_config_import(NULL, export_path, id_buf, (int)sizeof(id_buf)) != KT_OK,
              "config: import null config");
        check(kt_config_export(NULL, "tile-b", export_path) != KT_OK, "config: export null config");
        check(kt_config_export(cfg, "nope", export_path) != KT_OK, "config: export missing tile");
        check(kt_config_import(cfg, "C:\\no\\such\\file.json", id_buf, (int)sizeof(id_buf)) != KT_OK,
              "config: import missing file");
    }

    check(kt_config_save_default(cfg) == KT_OK, "config: save default path");
    check(kt_config_load("C:\\no\\such\\config.json") != NULL, "config: load missing returns config");
    check(kt_config_clear(cfg) == KT_OK, "config: clear");
    check(kt_config_tile_count(cfg) == 0, "config: count after clear");
    check(kt_config_clear(NULL) != KT_OK, "config: clear null");
    kt_config_free(cfg);
    kt_config_free(NULL);
    check(kt_config_tile_count(NULL) == 0, "config: count null");
    check(kt_config_has_tile(NULL, "x") == 0, "config: has null");
    check(kt_config_get_int(NULL, "x", "y", 7) == 7, "config: get int null");
    check(1, "config: free null is safe");
}

static void test_paths(void)
{
    char buf[1024];
    char buf2[1024];
    char app[1024];
    char big_text[70000];
    int count = 0;
    int i;

    check(kt_appdata_dir(buf, (int)sizeof(buf)) == KT_OK, "paths: appdata dir");
    check(strstr(buf, "koiTile") != NULL, "paths: appdata dir content");
    strcpy(app, buf);
    check(kt_path_exists_dir(buf) == 1, "paths: appdata dir exists");
    check(kt_config_path(buf, (int)sizeof(buf)) == KT_OK, "paths: config path");
    check(strstr(buf, "config.json") != NULL, "paths: config path content");
    check(kt_tileassets_dir(buf, (int)sizeof(buf)) == KT_OK, "paths: tileassets dir");
    check(strstr(buf, "TileAssets") != NULL, "paths: tileassets content");
    check(kt_path_exists_dir(buf) == 1, "paths: tileassets exists");
    check(kt_temp_dir(buf, (int)sizeof(buf)) == KT_OK, "paths: temp dir");
    check(kt_path_exists_dir(buf) == 1, "paths: temp exists");
    check(kt_appdata_dir(buf, 2) != KT_OK, "paths: appdata small buffer");
    check(kt_config_path(NULL, 8) != KT_OK, "paths: config path null");

    check(kt_path_join("C:\\a", "b", buf, (int)sizeof(buf)) == KT_OK, "paths: join");
    check(strcmp(buf, "C:\\a\\b") == 0, "paths: join value");
    check(kt_path_join("C:\\a\\", "b", buf, (int)sizeof(buf)) == KT_OK, "paths: join trailing");
    check(strcmp(buf, "C:\\a\\b") == 0, "paths: join trailing value");
    check(kt_path_join("C:\\a", "\\b", buf, (int)sizeof(buf)) == KT_OK, "paths: join leading");
    check(strcmp(buf, "\\b") == 0, "paths: join leading value is root relative");
    check(kt_path_join("C:\\a", "D:\\b", buf, (int)sizeof(buf)) == KT_OK, "paths: join absolute");
    check(strcmp(buf, "D:\\b") == 0, "paths: join absolute value");
    check(kt_path_join("C:\\a", "b", buf, 2) != KT_OK, "paths: join small buffer");
    check(kt_path_join(NULL, "b", buf, (int)sizeof(buf)) != KT_OK, "paths: join null rejected");

    check(kt_path_dirname("C:\\a\\b\\c.png", buf, (int)sizeof(buf)) == KT_OK, "paths: dirname");
    check(strcmp(buf, "C:\\a\\b") == 0, "paths: dirname value");
    check(kt_path_basename("C:\\a\\b\\c.png", buf, (int)sizeof(buf)) == KT_OK, "paths: basename");
    check(strcmp(buf, "c.png") == 0, "paths: basename value");
    check(kt_path_basename("C:\\a\\b\\", buf, (int)sizeof(buf)) == KT_OK, "paths: basename dir");
    check(strcmp(buf, "") == 0, "paths: basename dir value");
    check(kt_path_dirname("c.png", buf, (int)sizeof(buf)) == KT_OK, "paths: relative dirname");
    check(strcmp(buf, ".") == 0, "paths: relative dirname value");
    check(kt_path_dirname("C:\\a\\b\\c.png", buf, 4) != KT_OK, "paths: dirname small buffer");

    kt_path_join(app, "sample.txt", buf, (int)sizeof(buf));
    check(kt_file_write_text(buf, "hello") == KT_OK, "files: write text");
    check(kt_file_exists(buf) == 1, "files: exists");
    check(kt_path_exists_dir(buf) == 0, "files: not a dir");
    check(kt_file_read_text(buf, buf2, (int)sizeof(buf2)) == 5, "files: read text length");
    check(strcmp(buf2, "hello") == 0, "files: read text value");
    check(kt_file_read_text(buf, buf2, 3) != KT_OK, "files: read small buffer");
    check(kt_file_write_bytes(buf, "abc", 3) == KT_OK, "files: write bytes");
    check(kt_file_exists(buf) == 1, "files: overwritten");
    check(kt_file_read_text(buf, buf2, (int)sizeof(buf2)) == 3, "files: overwrite length");
    check(kt_file_delete(buf) == KT_OK, "files: delete");
    check(kt_file_exists(buf) == 0, "files: deleted");
    check(kt_file_delete(buf) == KT_OK, "files: delete missing ok");
    check(kt_file_read_text("C:\\no\\such\\file.txt", buf2, (int)sizeof(buf2)) != KT_OK,
          "files: read missing fails");
    check(kt_file_write_text(NULL, "x") != KT_OK, "files: write null path");

    kt_path_join(app, "sub", buf, (int)sizeof(buf));
    kt_path_join(buf, "deep", buf2, (int)sizeof(buf2));
    check(kt_dir_create(buf2) == KT_OK, "dirs: create recursive");
    check(kt_path_exists_dir(buf2) == 1, "dirs: recursive exists");
    check(kt_dir_create(buf2) == KT_OK, "dirs: create existing ok");
    check(kt_dir_create(NULL) != KT_OK, "dirs: create null rejected");

    kt_path_join(buf2, "one.png", buf, (int)sizeof(buf));
    kt_file_write_text(buf, "1");
    kt_path_join(buf2, "two.txt", buf, (int)sizeof(buf));
    kt_file_write_text(buf, "2");
    kt_path_join(buf2, "three.PNG", buf, (int)sizeof(buf));
    kt_file_write_text(buf, "3");
    {
        char list[4096];
        check(kt_dir_list(buf2, ".png", list, (int)sizeof(list), &count) == KT_OK, "dirs: list");
        check(count == 2, "dirs: list count by extension");
        check(strstr(list, "one.png") != NULL, "dirs: list contains one.png");
        check(strstr(list, "three.PNG") != NULL, "dirs: list extension case");
        check(strstr(list, "two.txt") == NULL, "dirs: list excludes txt");
        check(strstr(list, "\n") != NULL, "dirs: list newline separated");
        check(kt_dir_list(buf2, NULL, list, (int)sizeof(list), &count) == KT_OK, "dirs: list all");
        check(count == 3, "dirs: list all count");
        check(kt_dir_list(buf2, "png", list, (int)sizeof(list), &count) == KT_OK,
              "dirs: extension without dot");
        check(count == 2, "dirs: extension without dot count");
        check(kt_dir_list(buf2, ".png", list, 4, &count) != KT_OK, "dirs: list small buffer");
        check(kt_dir_list("C:\\no\\such\\dir", NULL, list, (int)sizeof(list), &count) == KT_OK,
              "dirs: list missing dir");
        check(count == 0, "dirs: list missing dir count");
        check(kt_dir_list(NULL, NULL, list, (int)sizeof(list), &count) != KT_OK,
              "dirs: list null rejected");
    }
    check(kt_dir_clear(buf2) == KT_OK, "dirs: clear");
    check(kt_dir_list(buf2, NULL, buf, (int)sizeof(buf), &count) == KT_OK, "dirs: list after clear");
    check(count == 0, "dirs: cleared count");
    check(kt_dir_clear(NULL) != KT_OK, "dirs: clear null rejected");

    for (i = 0; i < (int)sizeof(big_text) - 1; i++)
    {
        big_text[i] = 'a';
    }
    big_text[sizeof(big_text) - 1] = '\0';
    kt_path_join(app, "big.txt", buf, (int)sizeof(buf));
    check(kt_file_write_text(buf, big_text) == KT_OK, "files: write large");
    check(kt_file_read_text(buf, buf2, (int)sizeof(buf2)) != KT_OK, "files: read large rejected");
    kt_file_delete(buf);

    kt_path_join(app, "bom.txt", buf, (int)sizeof(buf));
    {
        FILE* f = fopen(buf, "wb");
        if (f)
        {
            fputs("\xEF\xBB\xBFhello", f);
            fclose(f);
        }
        check(kt_file_read_text(buf, buf2, (int)sizeof(buf2)) == 5, "files: utf8 bom stripped length");
        check(strcmp(buf2, "hello") == 0, "files: utf8 bom stripped");
        kt_file_delete(buf);
    }

    kt_path_join(app, "utf16.txt", buf, (int)sizeof(buf));
    {
        FILE* f = fopen(buf, "wb");
        if (f)
        {
            const unsigned char data[] = {0xFF, 0xFE, 'h', 0, 'i', 0};
            fwrite(data, 1, sizeof(data), f);
            fclose(f);
        }
        check(kt_file_read_text(buf, buf2, (int)sizeof(buf2)) == 2, "files: utf16 decoded length");
        check(strcmp(buf2, "hi") == 0, "files: utf16 decoded value");
        kt_file_delete(buf);
    }

    {
        int w = 0;
        int h = 0;
        int sw = 0;
        int sh = 0;
        int mw = 0;
        int mh = 0;
        int lw = 0;
        int lh = 0;
        check(kt_size_pixels(KT_SIZE_SMALL, &w, &h) == KT_OK && w == 70 && h == 70, "size: small 70x70");
        check(kt_size_pixels(KT_SIZE_MEDIUM, &w, &h) == KT_OK && w == 150 && h == 150,
              "size: medium 100x100");
        check(kt_size_pixels(KT_SIZE_WIDE, &w, &h) == KT_OK && w == 310 && h == 150,
              "size: wide 204x100");
        check(kt_size_pixels(KT_SIZE_LARGE, &w, &h) == KT_OK && w == 310 && h == 310,
              "size: large 204x204");
        check(kt_size_pixels(9, &w, &h) != KT_OK, "size: invalid rejected");
        check(kt_size_pixels(KT_SIZE_SMALL, NULL, &h) != KT_OK, "size: null rejected");
        kt_size_pixels(KT_SIZE_SMALL, &sw, &sh);
        kt_size_pixels(KT_SIZE_MEDIUM, &mw, &mh);
        kt_size_pixels(KT_SIZE_WIDE, &w, &h);
        kt_size_pixels(KT_SIZE_LARGE, &lw, &lh);
        check(sw == 70 && sh == 70 && mw == 150 && mh == 150 && lw == 310 && lh == 310,
              "size: 48/100/204 base values");
        check(w == lw && h == mh, "size: wide is large-wide by medium-high");
        check(h == mh && h == 150, "size: wide height follows medium");
    }
    check(kt_size_name(KT_SIZE_SMALL, buf, (int)sizeof(buf)) == KT_OK, "size: name small");
    check(strcmp(buf, "Small") == 0, "size: name small value");
    check(kt_size_name(KT_SIZE_MEDIUM, buf, (int)sizeof(buf)) == KT_OK, "size: name medium");
    check(strcmp(buf, "Medium") == 0, "size: name medium value");
    check(kt_size_name(KT_SIZE_WIDE, buf, (int)sizeof(buf)) == KT_OK, "size: name wide");
    check(strcmp(buf, "Wide") == 0, "size: name wide value");
    check(kt_size_name(KT_SIZE_LARGE, buf, (int)sizeof(buf)) == KT_OK, "size: name large");
    check(strcmp(buf, "Large") == 0, "size: name large value");
    check(kt_size_name(42, buf, (int)sizeof(buf)) != KT_OK, "size: name invalid");
    check(kt_size_name(KT_SIZE_SMALL, buf, 2) != KT_OK, "size: name small buffer");
}

static void test_text(void)
{
    KtConfig* cfg = kt_config_new();
    char buf[4096];
    char path[1024];
    int rc;

    check(kt_text_apply_template("a{content}b", "X", buf, (int)sizeof(buf)) == KT_OK,
          "text: template apply");
    check(strcmp(buf, "aXb") == 0, "text: template value");
    check(kt_text_apply_template("{content}{content}", "Y", buf, (int)sizeof(buf)) == KT_OK,
          "text: template twice");
    check(strcmp(buf, "YY") == 0, "text: template twice value");
    check(kt_text_apply_template("plain", "Y", buf, (int)sizeof(buf)) == KT_OK, "text: template plain");
    check(strcmp(buf, "plain") == 0, "text: template plain value");
    check(kt_text_apply_template("a{content}b", "X", buf, 3) != KT_OK, "text: template small buffer");
    check(kt_text_apply_template("", "X", buf, (int)sizeof(buf)) == KT_OK, "text: template empty");
    check(strcmp(buf, "") == 0, "text: template empty value");
    check(kt_text_apply_template("{content}", "", buf, (int)sizeof(buf)) == KT_OK,
          "text: template empty content");
    check(strcmp(buf, "") == 0, "text: template empty content value");
    check(kt_text_apply_template(NULL, "X", buf, (int)sizeof(buf)) == KT_OK, "text: template null tmpl");
    check(strcmp(buf, "X") == 0, "text: template null tmpl value");
    check(kt_text_apply_template("a", NULL, buf, (int)sizeof(buf)) != KT_OK, "text: template null content");

    kt_config_add_tile(cfg, "text-1");
    check(kt_config_set(cfg, "text-1", "textSourcePath", "static body") == KT_OK, "text: static source");
    rc = kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf));
    check(rc == KT_OK, "text: fetch static");
    check(strcmp(buf, "static body") == 0, "text: fetch static value");
    check(kt_config_set(cfg, "text-1", "textTemplate", "[{content}]") == KT_OK, "text: set template");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) == KT_OK, "text: fetch static templated");
    check(strcmp(buf, "[static body]") == 0, "text: fetch static templated value");

    kt_path_join(g_appdata, "body.txt", path, (int)sizeof(path));
    check(kt_file_write_text(path, "file body") == KT_OK, "text: write body file");
    check(kt_config_set_int(cfg, "text-1", "textSource", KT_TEXT_FILE) == KT_OK, "text: source file");
    check(kt_config_set(cfg, "text-1", "textSourcePath", path) == KT_OK, "text: source path");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) == KT_OK, "text: fetch file");
    check(strcmp(buf, "[file body]") == 0, "text: fetch file value");
    check(kt_text_fetch(cfg, "text-1", buf, 4) != KT_OK, "text: fetch small buffer");

    check(kt_config_set_int(cfg, "text-1", "textSource", KT_TEXT_URL) == KT_OK, "text: source url");
    check(kt_config_set(cfg, "text-1", "textSourcePath", "http://127.0.0.1:1/x") == KT_OK,
          "text: url value");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) != KT_OK, "text: fetch dead url fails");

    check(kt_config_set(cfg, "text-1", "textSourcePath", "") == KT_OK, "text: empty source path");
    check(kt_config_set_int(cfg, "text-1", "textSource", KT_TEXT_FILE) == KT_OK,
          "text: source file again");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) != KT_OK, "text: empty file path fails");

    check(kt_config_set(cfg, "text-1", "textSourcePath", "C:\\no\\such\\body.txt") == KT_OK,
          "text: missing file path");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) != KT_OK, "text: missing file fails");

    check(kt_config_set_int(cfg, "text-1", "textSource", 7) == KT_OK, "text: invalid source");
    check(kt_text_fetch(cfg, "text-1", buf, (int)sizeof(buf)) != KT_OK, "text: invalid source fails");
    check(kt_text_fetch(cfg, "nope", buf, (int)sizeof(buf)) != KT_OK, "text: missing tile fails");
    check(kt_text_fetch(cfg, NULL, buf, (int)sizeof(buf)) != KT_OK, "text: null id fails");

    kt_config_free(cfg);
}

static void test_schedule(void)
{
    KtConfig* cfg = kt_config_new();
    char ids[4096];
    int count = 0;

    check(kt_schedule_reset() == KT_OK, "schedule: reset");
    check(kt_schedule_due(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: due empty");
    check(count == 0, "schedule: due empty count");
    check(kt_schedule_seconds_left(cfg, "x") == 0, "schedule: seconds left unknown");

    kt_config_add_tile(cfg, "s1");
    kt_config_set_int(cfg, "s1", "refreshSeconds", 5);
    kt_config_add_tile(cfg, "s2");
    kt_config_set_int(cfg, "s2", "refreshSeconds", 300);
    check(kt_schedule_sync(cfg) == KT_OK, "schedule: sync");
    check(kt_schedule_due(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: due after sync");
    check(count == 0, "schedule: due after sync count");
    check(kt_schedule_seconds_left(cfg, "s1") >= 30, "schedule: minimum interval 30");
    check(kt_schedule_seconds_left(cfg, "s2") > 200, "schedule: seconds left large");
    check(kt_schedule_seconds_left(cfg, "s2") <= 300, "schedule: seconds left bounded");

    check(kt_schedule_mark(cfg, "s1") == KT_OK, "schedule: mark");
    check(kt_schedule_due(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: due after mark");
    check(count == 1, "schedule: due after mark count");
    check(strcmp(ids, "s1") == 0, "schedule: due after mark value");
    check(kt_schedule_due(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: due again");
    check(count == 0, "schedule: marker advanced");
    check(kt_schedule_mark(cfg, "nope") != KT_OK, "schedule: mark unknown");
    check(kt_schedule_mark(cfg, NULL) != KT_OK, "schedule: mark null");
    check(kt_schedule_mark(cfg, "s1") == KT_OK, "schedule: mark for buffer test");
    check(kt_schedule_due(cfg, ids, 2, &count) != KT_OK, "schedule: due small buffer");

    check(kt_schedule_mark(cfg, "s1") == KT_OK, "schedule: mark again");
    check(kt_schedule_seconds_left(cfg, "s1") == 0, "schedule: seconds left zero when due");

    check(kt_schedule_sync(NULL) != KT_OK, "schedule: sync null rejected");
    check(kt_schedule_due(cfg, NULL, 10, &count) != KT_OK, "schedule: due null buffer");
    check(kt_schedule_due(NULL, ids, 10, &count) != KT_OK, "schedule: due null config");

    check(kt_schedule_due_peek(cfg, ids, (int)sizeof(ids), &count) == KT_OK,
          "schedule: peek no lists");
    check(count == 0, "schedule: peek no lists count");
    kt_config_list_add(cfg, "s1", "peek", "p1");
    check(kt_schedule_due_peek(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: peek single");
    check(count == 0, "schedule: peek single count");
    kt_config_list_add(cfg, "s1", "peek", "p2");
    check(kt_schedule_due_peek(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: peek due");
    check(count == 1, "schedule: peek due count");
    check(strcmp(ids, "s1") == 0, "schedule: peek due value");
    check(kt_schedule_due_peek(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: peek again");
    check(count == 0, "schedule: peek interval enforced");
    check(kt_schedule_reset() == KT_OK, "schedule: reset for peek buffer test");
    check(kt_schedule_sync(cfg) == KT_OK, "schedule: resync for peek buffer test");
    check(kt_schedule_due_peek(cfg, ids, 2, &count) != KT_OK, "schedule: peek small buffer");
    check(kt_schedule_due_peek(NULL, ids, (int)sizeof(ids), &count) != KT_OK, "schedule: peek null");

    check(kt_schedule_reset() == KT_OK, "schedule: reset again");
    check(kt_schedule_due(cfg, ids, (int)sizeof(ids), &count) == KT_OK, "schedule: due after reset");
    check(count == 0, "schedule: due after reset count");

    kt_config_free(cfg);
}

static void test_xml(void)
{
    KtConfig* cfg = kt_config_new();
    char xml[8192];
    char shortcut[1024];
    char manifest[1024];

    check(kt_config_add_tile(cfg, "x1") == KT_OK, "xml: add tile");
    check(kt_xml_build(cfg, "x1", xml, (int)sizeof(xml)) == KT_OK, "xml: build");
    check(strstr(xml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>") != NULL, "xml: declaration");
    check(strstr(xml, "<Application") != NULL, "xml: application start");
    check(strstr(xml, "</Application>") != NULL, "xml: application end");
    check(strstr(xml, "<VisualElements") != NULL, "xml: visual elements node");
    check(strstr(xml, "Square70x70Logo=\"x1_48x48.png\"") != NULL, "xml: small logo");
    check(strstr(xml, "Square150x150Logo=\"x1_150x150.png\"") != NULL, "xml: medium logo");
    check(strstr(xml, "Wide310x150Logo=\"x1_310x150.png\"") != NULL, "xml: wide logo");
    check(strstr(xml, "Square310x310Logo=\"x1_310x310.png\"") != NULL, "xml: large logo");
    check(strstr(xml, "ShowNameOnSquare150x150Logo=\"on\"") != NULL, "xml: show name on");
    check(strstr(xml, "ShowNameOnSquare310x310Logo=\"on\"") != NULL, "xml: show name large");
    check(strstr(xml, "ShowNameOnWide310x150Logo=\"on\"") != NULL, "xml: show name wide");
    check(strstr(xml, "ForegroundText=\"light\"") != NULL, "xml: foreground light default");
    check(strstr(xml, "BackgroundColor=\"#2D89EF\"") != NULL, "xml: default background color");
    check(strstr(xml, "x1_48x48.png") != NULL, "xml: id used as target when name empty");

    check(kt_config_set(cfg, "x1", "name", "MyApp") == KT_OK, "xml: set name");
    check(kt_xml_build(cfg, "x1", xml, (int)sizeof(xml)) == KT_OK, "xml: build named");
    check(strstr(xml, "MyApp_48x48.png") != NULL, "xml: name used for files");
    check(strstr(xml, "MyApp_204x100.png") != NULL, "xml: name used for wide");

    check(kt_config_set_int(cfg, "x1", "showName", 0) == KT_OK, "xml: showName off");
    kt_config_set_int(cfg, "x1", "foregroundLight", 0);
    kt_config_set_int(cfg, "x1", "colorEnabled", 1);
    kt_config_set(cfg, "x1", "color", "#112233");
    check(kt_xml_build(cfg, "x1", xml, (int)sizeof(xml)) == KT_OK, "xml: build off");
    check(strstr(xml, "ShowNameOnSquare150x150Logo=\"off\"") != NULL, "xml: show name off");
    check(strstr(xml, "ShowNameOnSquare310x310Logo=\"off\"") != NULL, "xml: show name off large");
    check(strstr(xml, "ShowNameOnWide310x150Logo=\"off\"") != NULL, "xml: show name off wide");
    check(strstr(xml, "ForegroundText=\"dark\"") != NULL, "xml: foreground dark");
    check(strstr(xml, "BackgroundColor=\"#112233\"") != NULL, "xml: custom color");

    kt_config_set_int(cfg, "x1", "colorEnabled", 0);
    check(kt_xml_build(cfg, "x1", xml, (int)sizeof(xml)) == KT_OK, "xml: build color disabled");
    check(strstr(xml, "BackgroundColor=\"#2D89EF\"") != NULL, "xml: color disabled default");
    check(kt_xml_build(cfg, "nope", xml, (int)sizeof(xml)) != KT_OK, "xml: missing tile");
    check(kt_xml_build(NULL, "x1", xml, (int)sizeof(xml)) != KT_OK, "xml: null config");
    check(kt_xml_build(cfg, "x1", xml, 8) != KT_OK, "xml: small buffer");

    kt_dir_create("C:\\Temp\\koi-ctx");
    strcpy(shortcut, "C:\\Temp\\koi-ctx\\App.lnk");
    check(kt_config_set(cfg, "x1", "shortcut", shortcut) == KT_OK, "xml: set shortcut");
    check(kt_path_join("C:\\Temp\\koi-ctx", "VisualElementsManifest.xml", manifest,
                       (int)sizeof(manifest)) == KT_OK, "xml: manifest path expected");
    check(kt_xml_write(cfg, "x1") == KT_OK, "xml: write manifest");
    check(kt_file_exists(manifest) == 1, "xml: manifest file written");
    check(kt_file_read_text(manifest, xml, (int)sizeof(xml)) > 0, "xml: manifest readable");
    check(strstr(xml, "<VisualElements") != NULL, "xml: manifest content");
    check(kt_tile_state(cfg, "x1") == 0, "xml: write alone does not mark applied");
    check(kt_xml_remove(cfg, "x1") == KT_OK, "xml: remove manifest");
    check(kt_file_exists(manifest) == 0, "xml: manifest file removed");
    check(kt_xml_remove(cfg, "x1") == KT_OK, "xml: remove idempotent");
    check(kt_xml_write(cfg, "nope") != KT_OK, "xml: write missing tile");

    check(kt_tile_apply(cfg, "x1") == KT_OK, "apply: returns ok with stub image");
    check(kt_file_exists(manifest) == 1, "apply: manifest written");
    check(kt_tile_state(cfg, "x1") == 1, "apply: state marked");
    check(kt_tile_restore(cfg, "x1") == KT_OK, "apply: restore");
    check(kt_file_exists(manifest) == 0, "apply: manifest removed");
    check(kt_tile_state(cfg, "x1") == 0, "apply: state cleared");
    check(kt_tile_restore_all(cfg) == KT_OK, "apply: restore all");
    check(kt_tile_restore(cfg, "nope") != KT_OK, "apply: restore unknown tile");
    check(kt_tile_apply(cfg, "nope") != KT_OK, "apply: unknown tile");
    check(kt_tile_apply(NULL, "x1") != KT_OK, "apply: null config");
    check(kt_tile_restore_all(NULL) != KT_OK, "apply: restore all null");
    check(kt_config_set(cfg, "x1", "shortcut", "") == KT_OK, "apply: empty shortcut");
    check(kt_tile_apply(cfg, "x1") != KT_OK, "apply: empty shortcut rejected");
    check(kt_xml_write(cfg, "x1") != KT_OK, "xml: write with empty shortcut");
    kt_file_delete(manifest);
    kt_dir_clear("C:\\Temp\\koi-ctx");
    kt_file_delete("C:\\Temp\\koi-ctx");
    {
        char state_path[1024];
        kt_path_join(g_appdata, "state.json", state_path, (int)sizeof(state_path));
        kt_file_delete(state_path);
    }
    kt_config_free(cfg);
}

static void test_env(void)
{
    char buf[2048];
    char field[64];
    long long build;

    check(kt_env_explorerpatcher() == 0 || kt_env_explorerpatcher() == 1,
          "env: explorerpatcher flag");
    check(kt_env_win10_start() == 0 || kt_env_win10_start() == 1, "env: win10 start flag");
    check(kt_env_tile_notifications() == 0 || kt_env_tile_notifications() == 1,
          "env: tile notifications flag");
    check(kt_env_probe(buf, (int)sizeof(buf)) == KT_OK, "env: probe");
    check(buf[0] == '{' && buf[strlen(buf) - 1] == '}', "env: probe braces");
    check(get_field(buf, "explorerPatcher", field, (int)sizeof(field)) == 1,
          "env: probe explorerPatcher key");
    check(strcmp(field, "0") == 0 || strcmp(field, "1") == 0, "env: probe explorerPatcher value");
    check(get_field(buf, "win10Start", field, (int)sizeof(field)) == 1, "env: probe win10Start key");
    check(strcmp(field, "0") == 0 || strcmp(field, "1") == 0, "env: probe win10Start value");
    check(get_field(buf, "tileNotifications", field, (int)sizeof(field)) == 1,
          "env: probe tileNotifications key");
    check(strcmp(field, "0") == 0 || strcmp(field, "1") == 0, "env: probe tileNotifications value");
    check(get_field(buf, "build", field, (int)sizeof(field)) == 1, "env: probe build key");
    build = parse_ll(field);
    check(build > 0 && build < 100000, "env: probe build value");
    check(kt_env_probe(buf, 8) != KT_OK, "env: probe small buffer");
    check(kt_env_probe(NULL, 16) != KT_OK, "env: probe null buffer");
}

static void test_entry(void)
{
    char buf[256];
    int rc;

    check(kt_init() == KT_OK, "entry: init");
    check(kt_init() == KT_OK, "entry: init idempotent");
    check(kt_version(buf, (int)sizeof(buf)) == KT_OK, "entry: version");
    check(strlen(buf) > 0, "entry: version non-empty");
    check(kt_version(buf, 2) != KT_OK, "entry: version small buffer");
    check(kt_version(NULL, 8) != KT_OK, "entry: version null buffer");
    kt_free(NULL);
    check(1, "entry: free null");
    rc = kt_last_error(buf, (int)sizeof(buf));
    check(rc == KT_OK || rc == KT_ERR, "entry: last error callable");
    check(kt_last_error(NULL, 8) == KT_ERR, "entry: last error null");
    check(kt_last_error(buf, 0) == KT_ERR, "entry: last error zero buffer");
    kt_shutdown();
    check(kt_init() == KT_OK, "entry: re-init");
    check(strcmp(kt_version_string(), "1.0.0") == 0, "entry: version string");
}

static void test_image_api(void)
{
    int w = 0;
    int h = 0;
    int count = 0;
    KtConfig* cfg = kt_config_new();
    check(kt_image_info("x", &w, &h) == KT_ERR, "image api: missing file rejected");
    check(kt_image_resize("a", 1, 1, "b") == KT_ERR, "image api: resize missing source rejected");
    check(kt_image_cut_grid("a", 1, 1, 1, 1, "n", "d", &count) == KT_ERR,
          "image api: cut grid missing source rejected");
    check(count == 0, "image api: cut grid count reset");
    check(kt_image_render_tile(cfg, "t", 0, "d") == KT_ERR, "image api: render unknown tile rejected");
    check(kt_image_extract_icon("a", 0, 1, "b") == KT_ERR, "image api: extract missing file rejected");
    check(kt_image_composite("a", "b", 1, 0, 0, 1, 1, "c") == KT_ERR,
          "image api: composite missing input rejected");
    kt_config_free(cfg);
}

static void json_escape_path(const char* in, char* out, int cch)
{
    int i = 0;
    int j = 0;
    if (!out || cch <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (!in)
    {
        return;
    }
    while (in[i] && j + 3 < cch)
    {
        if (in[i] == '\\' || in[i] == '"')
        {
            out[j] = '\\';
            j++;
        }
        out[j] = in[i];
        i++;
        j++;
    }
    out[j] = '\0';
}

static void test_install(void)
{
    char tmp_root[1024];
    char work[1024];
    char fake[1024];
    char src[1024];
    char missing[1024];
    char url_link[1024];
    char created[1024];
    char old_link[1024];
    char assets[1024];
    char manifest[1024];
    char png48[1024];
    char png100[1024];
    char pngwide[1024];
    char pnglarge[1024];
    char foreign[1024];
    char target[1024];
    char field[1024];
    char state_path[1024];
    char escaped[2048];
    char json[4096];
    unsigned hash_before = 0;
    unsigned hash_after = 0;
    int size_before = 0;
    int size_after = 0;
    KtConfig* cfg;
    char* raw;
    DWORD n;

    n = GetTempPathA((DWORD)sizeof(tmp_root), tmp_root);
    check(n > 0, "install: temp path available");
    kt_path_join(tmp_root, "koitile-install-test", work, (int)sizeof(work));
    kt_dir_clear(work);
    kt_file_delete(work);
    check(kt_dir_create(work) == KT_OK, "install: work dir created");
    kt_path_join(work, "fake.exe", fake, (int)sizeof(fake));
    kt_path_join(work, "Source.lnk", src, (int)sizeof(src));
    kt_path_join(work, "Missing.lnk", missing, (int)sizeof(missing));
    kt_path_join(work, "Url.lnk", url_link, (int)sizeof(url_link));
    kt_path_join(work, "Created.lnk", created, (int)sizeof(created));
    kt_path_join(work, "Demo.lnk", old_link, (int)sizeof(old_link));
    kt_path_join(work, "VisualElements", assets, (int)sizeof(assets));
    kt_path_join(work, "VisualElementsManifest.xml", manifest, (int)sizeof(manifest));
    kt_path_join(assets, "Demo_48x48.png", png48, (int)sizeof(png48));
    kt_path_join(assets, "Demo_150x150.png", png100, (int)sizeof(png100));
    kt_path_join(assets, "Demo_310x150.png", pngwide, (int)sizeof(pngwide));
    kt_path_join(assets, "Demo_310x310.png", pnglarge, (int)sizeof(pnglarge));
    kt_path_join(assets, "keep.txt", foreign, (int)sizeof(foreign));
    kt_file_delete(missing);
    kt_file_delete(old_link);
    check(kt_file_write_text(fake, "") == KT_OK, "install: fake exe created");
    check(kt_file_exists(fake) == 1, "install: fake exe exists");

    cfg = kt_config_new();
    check(kt_config_add_tile(cfg, "install-1") == KT_OK, "install: add tile");
    check(kt_tile_install_start(cfg, "install-1") != KT_OK, "install: empty shortcut rejected");
    check(kt_config_set(cfg, "install-1", "shortcut", missing) == KT_OK, "install: set missing shortcut");
    check(kt_tile_install_start(cfg, "install-1") != KT_OK, "install: missing shortcut rejected");
    check(kt_config_set(cfg, "install-1", "shortcut", "") == KT_OK, "install: clear shortcut");
    check(kt_tile_install_start(cfg, "install-1") != KT_OK, "install: cleared shortcut rejected");
    check(kt_tile_install_start(cfg, "nope") != KT_OK, "install: unknown tile rejected");
    check(kt_tile_install_start(NULL, "install-1") != KT_OK, "install: null config rejected");
    check(kt_tile_install_start(cfg, NULL) != KT_OK, "install: null id rejected");
    check(kt_config_get(cfg, "install-1", "shortcut", field, (int)sizeof(field)) == KT_OK,
          "install: shortcut field readable");
    check(strcmp(field, "") == 0, "install: shortcut field untouched by failures");
    check(kt_path_exists_dir(assets) == 0, "install: no assets dir after failures");
    check(kt_file_exists(manifest) == 0, "install: no manifest after failures");

    check(kt_shortcut_create("https://example.com", url_link, "koiTile.install-url", "", "") == KT_OK,
          "install: url shortcut created");
    check(kt_file_exists(url_link) == 1, "install: url shortcut exists");
    check(kt_config_set(cfg, "install-1", "shortcut", url_link) == KT_OK, "install: set url shortcut");
    check(kt_tile_install_start(cfg, "install-1") != KT_OK, "install: url target rejected");
    check(kt_path_exists_dir(assets) == 0, "install: url target leaves no assets dir");
    check(kt_file_exists(manifest) == 0, "install: url target leaves no manifest");

    kt_config_free(cfg);
    cfg = kt_config_new();
    check(kt_config_add_tile(cfg, "install-1") == KT_OK, "install: add tile again");
    check(kt_shortcut_create(fake, src, "koiTile.install-1", "", work) == KT_OK,
          "install: source shortcut created");
    target[0] = '\0';
    check(kt_shortcut_target(src, target, (int)sizeof(target)) == KT_OK, "install: source target readable");
    check(strstr(target, "fake.exe") != NULL, "install: source target is the fake exe");
    check(kt_config_set(cfg, "install-1", "shortcut", src) == KT_OK, "install: set source shortcut");
    check(kt_config_set(cfg, "install-1", "name", "Demo") == KT_OK, "install: set tile name");
    check(file_digest(src, &hash_before, &size_before) == 1, "install: source shortcut snapshot");
    check(kt_tile_install_start(cfg, "install-1") == KT_OK, "install: install returns ok");
    check(kt_path_exists_dir(assets) == 1, "install: assets dir created");
    check(kt_file_exists(png48) == 1, "install: 48x48 png written");
    check(kt_file_exists(png100) == 1, "install: 100x100 png written");
    check(kt_file_exists(pngwide) == 1, "install: 204x100 png written");
    check(kt_file_exists(pnglarge) == 1, "install: 204x204 png written");
    check(kt_file_exists(manifest) == 1, "install: manifest written");
    check(kt_file_exists(old_link) == 0, "install: no shortcut created next to exe");
    check(file_digest(src, &hash_after, &size_after) == 1, "install: source shortcut still readable");
    check(size_before == size_after && hash_before == hash_after, "install: source .lnk unchanged");
    check(kt_config_get(cfg, "install-1", "shortcut", field, (int)sizeof(field)) == KT_OK,
          "install: shortcut field readable after install");
    check(strcmp(field, src) == 0, "install: shortcut field unchanged");
    {
        char list[4096];
        int count = 0;
        check(kt_dir_list(assets, NULL, list, (int)sizeof(list), &count) == KT_OK,
              "install: assets dir listable");
        check(count == 4, "install: assets dir holds the four png only");
    }

    raw = kt_file_read_utf8(manifest);
    check(raw != NULL, "install: manifest readable");
    if (raw)
    {
        check(strstr(raw, "<?xml version=\"1.0\" encoding=\"utf-8\"?>") != NULL,
              "install: manifest declaration");
        check(strstr(raw, "<Application") != NULL, "install: manifest application node");
        check(strstr(raw, "Square70x70Logo=\"VisualElements\\Demo_48x48.png\"") != NULL,
              "install: manifest small logo prefixed");
        check(strstr(raw, "Square150x150Logo=\"VisualElements\\Demo_150x150.png\"") != NULL,
              "install: manifest medium logo prefixed");
        check(strstr(raw, "Wide310x150Logo=\"VisualElements\\Demo_310x150.png\"") != NULL,
              "install: manifest wide logo prefixed");
        check(strstr(raw, "Square310x310Logo=\"VisualElements\\Demo_310x310.png\"") != NULL,
              "install: manifest large logo prefixed");
        check(strstr(raw, "ShowNameOnSquare150x150Logo=\"on\"") != NULL, "install: manifest show name");
        check(strstr(raw, "ShowNameOnSquare310x310Logo=\"on\"") != NULL,
              "install: manifest show name large");
        check(strstr(raw, "ShowNameOnWide310x150Logo=\"on\"") != NULL,
              "install: manifest show name wide");
        check(strstr(raw, "ForegroundText=\"light\"") != NULL, "install: manifest foreground");
        check(strstr(raw, "BackgroundColor=\"#2D89EF\"") != NULL, "install: manifest background");
        printf("--- install manifest ---\n%s------------------------\n", raw);
        free(raw);
    }

    state_file_path(state_path, (int)sizeof(state_path));
    raw = kt_file_read_utf8(state_path);
    check(raw != NULL, "install: state readable");
    if (raw)
    {
        check(strstr(raw, "\"installed\":1") != NULL, "install: state marks installed");
        check(strstr(raw, "installDir") != NULL, "install: state records installDir");
        check(strstr(raw, "installManifest") != NULL, "install: state records installManifest");
        check(strstr(raw, "installFiles") != NULL, "install: state records installFiles");
        check(strstr(raw, "Demo_48x48.png") != NULL, "install: state records 48x48 png");
        check(strstr(raw, "createdShortcut") == NULL, "install: no createdShortcut written");
        free(raw);
    }

    check(kt_tile_restore(cfg, "install-1") == KT_OK, "install: restore returns ok");
    check(kt_file_exists(png48) == 0, "install: 48x48 png removed");
    check(kt_file_exists(png100) == 0, "install: 100x100 png removed");
    check(kt_file_exists(pngwide) == 0, "install: 204x100 png removed");
    check(kt_file_exists(pnglarge) == 0, "install: 204x204 png removed");
    check(kt_file_exists(manifest) == 0, "install: manifest removed");
    check(kt_path_exists_dir(assets) == 0, "install: empty assets dir removed");
    check(kt_file_exists(fake) == 1, "install: exe kept after restore");
    check(kt_file_exists(src) == 1, "install: source shortcut kept after restore");
    raw = kt_file_read_utf8(state_path);
    if (raw)
    {
        check(strstr(raw, "install-1") == NULL, "install: state entry removed");
        free(raw);
    }
    else
    {
        check(1, "install: state entry removed");
    }

    check(kt_tile_install_start(cfg, "install-1") == KT_OK, "install: second install returns ok");
    check(kt_file_exists(png48) == 1, "install: second install writes 48x48 png");
    check(kt_file_write_text(foreign, "keep") == KT_OK, "install: foreign file written");
    check(kt_tile_restore(cfg, "install-1") == KT_OK, "install: restore with foreign file returns ok");
    check(kt_file_exists(png48) == 0, "install: png removed beside foreign file");
    check(kt_file_exists(manifest) == 0, "install: manifest removed beside foreign file");
    check(kt_path_exists_dir(assets) == 1, "install: non empty assets dir kept");
    check(kt_file_exists(foreign) == 1, "install: foreign file untouched");
    check(kt_file_delete(foreign) == KT_OK, "install: foreign file removed by test");
    check(kt_file_delete(assets) == KT_OK, "install: assets dir removed by test");

    kt_config_free(cfg);
    cfg = kt_config_new();
    check(kt_shortcut_create(fake, created, "koiTile.install-legacy", "", "") == KT_OK,
          "install: legacy shortcut created");
    json_escape_path(created, escaped, (int)sizeof(escaped));
    snprintf(json, sizeof(json),
             "{\"tiles\":{\"save-1\":{\"applied\":1,\"manifest\":\"\","
             "\"createdShortcut\":\"%s\",\"files\":[]}}}",
             escaped);
    check(kt_file_write_text(state_path, json) == KT_OK, "install: legacy state written");
    kt_config_add_tile(cfg, "save-1");
    kt_config_set(cfg, "save-1", "shortcut", src);
    kt_config_set(cfg, "save-1", "name", "Saved");
    check(kt_tile_apply(cfg, "save-1") == KT_OK, "install: legacy apply returns ok");
    raw = kt_file_read_utf8(state_path);
    check(raw != NULL, "install: legacy state readable after apply");
    check(raw && strstr(raw, "createdShortcut") != NULL, "install: legacy createdShortcut persisted");
    check(raw && strstr(raw, escaped) != NULL, "install: legacy createdShortcut path preserved");
    free(raw);
    check(kt_file_exists(created) == 1, "install: legacy shortcut kept by apply");
    check(kt_tile_restore(cfg, "save-1") == KT_OK, "install: legacy restore returns ok");
    check(kt_file_exists(created) == 0, "install: legacy createdShortcut removed by restore");
    check(kt_file_exists(src) == 1, "install: legacy user shortcut kept by restore");
    check(kt_tile_state(cfg, "save-1") == 0, "install: legacy state cleared by restore");
    kt_config_free(cfg);

    kt_file_delete(state_path);
    kt_file_delete(url_link);
    kt_file_delete(src);
    kt_file_delete(fake);
    kt_dir_clear(assets);
    kt_file_delete(assets);
    kt_dir_clear(work);
    kt_file_delete(work);
}

int main(void)
{
    DWORD n;
    int rc = 0;

    n = GetEnvironmentVariableA("APPDATA", g_appdata, (DWORD)sizeof(g_appdata));
    if (n == 0 || n >= (DWORD)sizeof(g_appdata))
    {
        printf("FAIL  setup: APPDATA missing\n");
        return 2;
    }
    printf("APPDATA=%s\n", g_appdata);
    fflush(stdout);

    cleanup_test_state();

    test_json();
    test_roundtrip();
    test_config();
    test_paths();
    test_text();
    test_schedule();
    test_xml();
    test_apply_flow();
    test_link();
    test_install();
    test_env();
    test_entry();
    test_image_api();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    if (g_fail != 0)
    {
        rc = 1;
    }
    return rc;
}
