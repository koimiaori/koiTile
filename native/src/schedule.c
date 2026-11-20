#include "koitile_internal.h"

static KtScheduleEntry* g_schedule = NULL;
static int g_schedule_count = 0;
static int g_schedule_capacity = 0;
static ULONGLONG g_peek_next = 0;
static int g_peek_init = 0;

static ULONGLONG kt_now_ms(void)
{
    return GetTickCount64();
}

static int kt_clamp_seconds(int v, int minimum)
{
    if (v < minimum)
    {
        return minimum;
    }
    return v;
}

static int kt_schedule_append(char*** list, int* count, const char* s)
{
    char** grown;
    char* copy;
    grown = (char**)realloc(*list, ((size_t)*count + 1) * sizeof(char*));
    if (!grown)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    *list = grown;
    copy = kt_strdup(s);
    if (!copy)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    grown[*count] = copy;
    *count = *count + 1;
    return KT_OK;
}

int kt_schedule_reset(void)
{
    int i;
    for (i = 0; i < g_schedule_count; i++)
    {
        free(g_schedule[i].id);
    }
    free(g_schedule);
    g_schedule = NULL;
    g_schedule_count = 0;
    g_schedule_capacity = 0;
    g_peek_next = kt_now_ms();
    g_peek_init = 0;
    return KT_OK;
}

static KtScheduleEntry* kt_schedule_entry(const char* id)
{
    int i;
    if (!id)
    {
        return NULL;
    }
    for (i = 0; i < g_schedule_count; i++)
    {
        if (kt_streq(g_schedule[i].id, id))
        {
            return &g_schedule[i];
        }
    }
    return NULL;
}

int kt_schedule_sync(KtConfig* cfg)
{
    KtScheduleEntry* grown;
    int cap;
    int total;
    int i;
    ULONGLONG now;
    if (!cfg)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    now = kt_now_ms();
    total = kt_config_tile_count(cfg);
    for (i = 0; i < g_schedule_count; i++)
    {
        free(g_schedule[i].id);
    }
    g_schedule_count = 0;
    if (total > g_schedule_capacity)
    {
        cap = total + 8;
        grown = (KtScheduleEntry*)realloc(g_schedule, (size_t)cap * sizeof(KtScheduleEntry));
        if (!grown)
        {
            free(g_schedule);
            g_schedule = NULL;
            g_schedule_capacity = 0;
            kt_err("out of memory");
            return KT_ERR;
        }
        g_schedule = grown;
        g_schedule_capacity = cap;
    }
    for (i = 0; i < total; i++)
    {
        char id[256];
        int seconds;
        if (kt_config_tile_id_at(cfg, i, id, 256) != KT_OK)
        {
            continue;
        }
        seconds = kt_clamp_seconds(kt_config_get_int(cfg, id, "refreshSeconds", 30), 30);
        g_schedule[g_schedule_count].id = kt_strdup(id);
        if (!g_schedule[g_schedule_count].id)
        {
            kt_err("out of memory");
            return KT_ERR;
        }
        g_schedule[g_schedule_count].refresh_seconds = seconds;
        g_schedule[g_schedule_count].next_refresh = now + (ULONGLONG)seconds * 1000ull;
        g_schedule_count++;
    }
    if (!g_peek_init)
    {
        g_peek_next = now;
        g_peek_init = 1;
    }
    return KT_OK;
}

int kt_schedule_due(KtConfig* cfg, char* ids, int cch, int* count)
{
    ULONGLONG now;
    char** list = NULL;
    int n = 0;
    int i;
    int j;
    KtBuf b;
    int rc = KT_OK;
    if (count)
    {
        *count = 0;
    }
    if (!cfg || !ids || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    ids[0] = '\0';
    now = kt_now_ms();
    for (i = 0; i < g_schedule_count; i++)
    {
        if (g_schedule[i].next_refresh <= now)
        {
            if (kt_schedule_append(&list, &n, g_schedule[i].id) != KT_OK)
            {
                for (j = 0; j < n; j++)
                {
                    free(list[j]);
                }
                free(list);
                return KT_ERR;
            }
            g_schedule[i].next_refresh =
                now + (ULONGLONG)kt_clamp_seconds(g_schedule[i].refresh_seconds, 30) * 1000ull;
        }
    }
    kt_buf_init(&b, ids, cch);
    for (i = 0; i < n; i++)
    {
        if (i)
        {
            if (kt_buf_addc(&b, '\n') != KT_OK)
            {
                rc = KT_ERR;
                break;
            }
        }
        if (kt_buf_add(&b, list[i]) != KT_OK)
        {
            rc = KT_ERR;
            break;
        }
    }
    if (kt_buf_finish(&b) != KT_OK)
    {
        rc = KT_ERR;
    }
    for (j = 0; j < n; j++)
    {
        free(list[j]);
    }
    free(list);
    if (count)
    {
        *count = n;
    }
    return rc;
}

int kt_schedule_mark(KtConfig* cfg, const char* id)
{
    KtScheduleEntry* e;
    if (!cfg || !id || !id[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    e = kt_schedule_entry(id);
    if (!e)
    {
        kt_err("tile not scheduled");
        return KT_ERR;
    }
    e->next_refresh = kt_now_ms();
    return KT_OK;
}

int kt_schedule_seconds_left(KtConfig* cfg, const char* id)
{
    KtScheduleEntry* e;
    ULONGLONG now;
    ULONGLONG left;
    if (!cfg || !id || !id[0])
    {
        return 0;
    }
    e = kt_schedule_entry(id);
    if (!e)
    {
        return 0;
    }
    now = kt_now_ms();
    if (e->next_refresh <= now)
    {
        return 0;
    }
    left = e->next_refresh - now;
    left = (left + 999ull) / 1000ull;
    if (left > 2147483647ull)
    {
        return 2147483647;
    }
    return (int)left;
}

int kt_schedule_due_peek(KtConfig* cfg, char* ids, int cch, int* count)
{
    ULONGLONG now;
    char** list = NULL;
    int n = 0;
    int total;
    int i;
    int j;
    KtBuf b;
    int rc = KT_OK;
    if (count)
    {
        *count = 0;
    }
    if (!cfg || !ids || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    ids[0] = '\0';
    if (!g_peek_init)
    {
        g_peek_next = kt_now_ms();
        g_peek_init = 1;
    }
    total = kt_config_tile_count(cfg);
    for (i = 0; i < total; i++)
    {
        char id[256];
        int peek_count;
        if (kt_config_tile_id_at(cfg, i, id, 256) != KT_OK)
        {
            continue;
        }
        peek_count = kt_config_list_count(cfg, id, "peek");
        if (peek_count <= 1)
        {
            continue;
        }
        if (kt_schedule_append(&list, &n, id) != KT_OK)
        {
            for (j = 0; j < n; j++)
            {
                free(list[j]);
            }
            free(list);
            return KT_ERR;
        }
    }
    now = kt_now_ms();
    if (n == 0 || g_peek_next > now)
    {
        for (j = 0; j < n; j++)
        {
            free(list[j]);
        }
        free(list);
        return KT_OK;
    }
    for (i = 0; i < n; i++)
    {
        int seconds = kt_clamp_seconds(kt_config_get_int(cfg, list[i], "peekSeconds", 12), 5);
        g_peek_next = now + (ULONGLONG)seconds * 1000ull;
    }
    kt_buf_init(&b, ids, cch);
    for (i = 0; i < n; i++)
    {
        if (i)
        {
            if (kt_buf_addc(&b, '\n') != KT_OK)
            {
                rc = KT_ERR;
                break;
            }
        }
        if (kt_buf_add(&b, list[i]) != KT_OK)
        {
            rc = KT_ERR;
            break;
        }
    }
    if (kt_buf_finish(&b) != KT_OK)
    {
        rc = KT_ERR;
    }
    for (j = 0; j < n; j++)
    {
        free(list[j]);
    }
    free(list);
    if (count)
    {
        *count = n;
    }
    return rc;
}
