#include "koitile_internal.h"

static int g_com_initialized = 0;
static int g_initialized = 0;
static const char kt_version_text[] = "1.0.0";

const char* kt_version_string(void)
{
    return kt_version_text;
}

int kt_version(char* buf, int cch)
{
    int n;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    n = (int)strlen(kt_version_text);
    if (n + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    memcpy(buf, kt_version_text, (size_t)n + 1);
    return KT_OK;
}

int kt_init(void)
{
    HRESULT hr;
    char dir[KT_MAX_PATHBUF];
    if (g_initialized)
    {
        return KT_OK;
    }
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        g_com_initialized = 1;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        kt_err("COM initialization failed");
        return KT_ERR;
    }
    if (kt_appdata_dir(dir, KT_MAX_PATHBUF) != KT_OK)
    {
        kt_err("appdata directory unavailable");
        return KT_ERR;
    }
    if (kt_tileassets_dir(dir, KT_MAX_PATHBUF) != KT_OK)
    {
        kt_err("tile assets directory unavailable");
        return KT_ERR;
    }
    if (kt_temp_dir(dir, KT_MAX_PATHBUF) != KT_OK)
    {
        kt_err("temp directory unavailable");
        return KT_ERR;
    }
    kt_schedule_reset();
    kt_err(NULL);
    g_initialized = 1;
    return KT_OK;
}

void kt_shutdown(void)
{
    kt_schedule_reset();
    if (g_com_initialized)
    {
        CoUninitialize();
        g_com_initialized = 0;
    }
    g_initialized = 0;
}

void kt_free(void* p)
{
    free(p);
}

int kt_last_error(char* buf, int cch)
{
    const char* text = kt_error_text_copy();
    int n;
    if (!buf || cch <= 0)
    {
        return KT_ERR;
    }
    buf[0] = '\0';
    n = (int)strlen(text);
    if (n == 0)
    {
        return KT_ERR;
    }
    if (n + 1 > cch)
    {
        return KT_ERR;
    }
    memcpy(buf, text, (size_t)n + 1);
    return KT_OK;
}
