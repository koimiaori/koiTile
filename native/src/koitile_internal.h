#ifndef KOITILE_INTERNAL_H
#define KOITILE_INTERNAL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <propkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "../include/koitile_core.h"

#define KT_MAX_TEXT 65536
#define KT_MAX_HTTP 262144
#define KT_MAX_PATHBUF 2048

#define KT_JSON_NULL 0
#define KT_JSON_BOOL 1
#define KT_JSON_NUMBER 2
#define KT_JSON_STRING 3
#define KT_JSON_ARRAY 4
#define KT_JSON_OBJECT 5

typedef struct KtJson KtJson;
typedef struct KtJsonMember KtJsonMember;
typedef struct KtConfig KtConfig;
typedef struct KtTile KtTile;
typedef struct KtCfgEntry KtCfgEntry;
typedef struct KtCfgList KtCfgList;
typedef struct KtScheduleEntry KtScheduleEntry;
typedef struct KtStateTile KtStateTile;

struct KtJsonMember
{
    char* key;
    KtJson* value;
    KtJsonMember* next;
};

struct KtJson
{
    int type;
    long long number;
    char* text;
    KtJson** items;
    int count;
    int capacity;
    KtJsonMember* members;
    KtJsonMember* members_tail;
};

struct KtCfgEntry
{
    char* key;
    char* value;
    KtCfgEntry* next;
};

struct KtCfgList
{
    KtCfgEntry* head;
    KtCfgEntry* tail;
    int count;
};

struct KtTile
{
    char* id;
    KtCfgList fields;
};

struct KtConfig
{
    KtTile** tiles;
    int count;
    int capacity;
    KtTile* board;
};

struct KtScheduleEntry
{
    char* id;
    int refresh_seconds;
    ULONGLONG next_refresh;
};

struct KtStateTile
{
    char* id;
    int applied;
    char* manifest;
    char* orig_target;
    char* orig_args;
    char* orig_work_dir;
    char* created_shortcut;
    char** files;
    int file_count;
    int installed;
    char* install_dir;
    char* install_manifest;
    char** install_files;
    int install_file_count;
    KtStateTile* next;
};

typedef struct KtState
{
    KtStateTile* head;
    KtStateTile* tail;
} KtState;

typedef struct KtBuf
{
    char* buf;
    int cch;
    int len;
    int growable;
} KtBuf;

void kt_err(const char* message);
const char* kt_error_text_copy(void);
const char* kt_version_string(void);
void kt_buf_init(KtBuf* b, char* buf, int cch);
int kt_buf_add(KtBuf* b, const char* s);
int kt_buf_addn(KtBuf* b, const char* s, int n);
int kt_buf_addc(KtBuf* b, char c);
int kt_buf_result(KtBuf* b);
int kt_buf_finish(KtBuf* b);
char* kt_strdup(const char* s);
char* kt_strndup(const char* s, int n);
int kt_streq(const char* a, const char* b);
int kt_starts_with(const char* s, const char* prefix);
int kt_ends_with(const char* s, const char* suffix);
char* kt_trim(char* s);
int kt_append(char* buf, int cch, const char* s);

wchar_t* kt_utf8_to_utf16(const char* s);
char* kt_utf16_to_utf8(const wchar_t* w);
char* kt_utf16_to_utf8_len(const wchar_t* w, int wlen);
int kt_utf16_to_utf8_buf(const wchar_t* w, int wlen, char* buf, int cch);

KtJson* kt_json_parse(const char* text);
KtJson* kt_json_parse_file(const char* path);
void kt_json_free(KtJson* v);
KtJson* kt_json_new_object(void);
KtJson* kt_json_new_array(void);
KtJson* kt_json_new_string(const char* s);
KtJson* kt_json_new_number(long long n);
KtJson* kt_json_new_bool(int b);
KtJsonMember* kt_json_member(KtJson* obj, const char* key);
KtJson* kt_json_get(KtJson* obj, const char* key);
const char* kt_json_get_string(KtJson* obj, const char* key, const char* fallback);
int kt_json_get_int(KtJson* obj, const char* key, int fallback);
void kt_json_set_string(KtJson* obj, const char* key, const char* value);
void kt_json_set_int(KtJson* obj, const char* key, int value);
void kt_json_set(KtJson* obj, const char* key, KtJson* value);
void kt_json_add(KtJson* arr, KtJson* value);
char* kt_json_dump(KtJson* v);

char* kt_config_serialize(KtConfig* cfg);
char* kt_tile_serialize(KtTile* tile);
KtTile* kt_tile_from_json(KtJson* obj);
KtConfig* kt_config_from_json(KtJson* root);
void kt_tile_free(KtTile* tile);
KtTile* kt_config_find(KtConfig* cfg, const char* id);
int kt_config_ensure(KtConfig* cfg, const char* id);
KtTile* kt_config_board_slot(KtConfig* cfg);
void kt_config_remove_at(KtConfig* cfg, int index);
KtJson* kt_list_json(KtConfig* cfg, const char* id, const char* key, int create);
KtCfgEntry* kt_tile_field(KtTile* tile, const char* key, int create);
int kt_clamp_int(int v, int lo, int hi);

char* kt_file_read_utf8(const char* path);
int kt_json_utf8_dec(const char* s, unsigned* cp);
int kt_file_list_add(char*** list, int* count, const char* s);
void kt_file_list_free(char** list, int count);

int kt_registry_key_exists(HKEY root, const wchar_t* subkey);
int kt_registry_read_int(HKEY root, const wchar_t* subkey, const wchar_t* name, int fallback);
int kt_windows_build(void);

KtState* kt_state_load(void);
void kt_state_free(KtState* st);
KtStateTile* kt_state_find(KtState* st, const char* id, int create);
void kt_state_drop(KtStateTile* t);
int kt_state_save(KtState* st);
int kt_manifest_path(const char* shortcut, char* buf, int cch);
int kt_tile_private_name(const char* path, char* buf, int cch);
int kt_logical_dir(const char* path, char* buf, int cch);
int kt_tile_file_target(KtConfig* cfg, KtTile* tile, char* buf, int cch);
int kt_dir_empty(const char* path);
int kt_start_apps_dir(char* buf, int cch);
int kt_path_is_local(const char* path);

int kt_xml_build_tile(KtConfig* cfg, const char* id, char* buf, int cch);
int kt_xml_build_prefixed(KtConfig* cfg, const char* id, const char* prefix, char* buf, int cch);
int kt_size_file_name(int size, char* buf, int cch);
int kt_shortcut_target(const char* link, char* buf, int cch);
int kt_shortcut_args(const char* link, char* buf, int cch);
int kt_shortcut_work_dir(const char* link, char* buf, int cch);
int kt_state_set_origin(KtStateTile* t, const char* target, const char* args, const char* work_dir);

#endif
