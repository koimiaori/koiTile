#include "koitile_internal.h"

int kt_registry_key_exists(HKEY root, const wchar_t* subkey)
{
    HKEY key = NULL;
    LONG rc;
    if (!subkey)
    {
        return 0;
    }
    rc = RegOpenKeyExW(root, subkey, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS)
    {
        return 0;
    }
    RegCloseKey(key);
    return 1;
}

int kt_registry_read_int(HKEY root, const wchar_t* subkey, const wchar_t* name, int fallback)
{
    HKEY key = NULL;
    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    LONG rc;
    if (!subkey || !name)
    {
        return fallback;
    }
    rc = RegOpenKeyExW(root, subkey, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS)
    {
        return fallback;
    }
    rc = RegQueryValueExW(key, name, NULL, &type, (LPBYTE)&value, &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_DWORD)
    {
        return fallback;
    }
    return (int)value;
}

static int kt_read_build_name(HKEY key, const wchar_t* name)
{
    DWORD type = 0;
    BYTE data[64];
    DWORD size = sizeof(data);
    LONG rc;
    wchar_t text[64];
    const wchar_t* src;
    int i = 0;
    memset(data, 0, sizeof(data));
    rc = RegQueryValueExW(key, name, NULL, &type, data, &size);
    if (rc != ERROR_SUCCESS || type != REG_SZ || size == 0)
    {
        return 0;
    }
    src = (const wchar_t*)(const void*)data;
    while (src[i] && i < 63)
    {
        text[i] = src[i];
        i++;
    }
    text[i] = L'\0';
    return _wtoi(text);
}

int kt_windows_build(void)
{
    HKEY key = NULL;
    LONG rc;
    int build;
    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                       KEY_READ | KEY_WOW64_64KEY, &key);
    if (rc != ERROR_SUCCESS)
    {
        return 0;
    }
    build = kt_read_build_name(key, L"CurrentBuildNumber");
    if (build <= 0)
    {
        build = kt_read_build_name(key, L"CurrentBuild");
    }
    RegCloseKey(key);
    return build;
}

int kt_env_explorerpatcher(void)
{
    if (kt_registry_key_exists(HKEY_CURRENT_USER, L"Software\\ExplorerPatcher"))
    {
        return 1;
    }
    if (kt_registry_key_exists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ExplorerPatcher"))
    {
        return 1;
    }
    return 0;
}

int kt_env_win10_start(void)
{
    if (kt_registry_read_int(HKEY_CURRENT_USER, L"Software\\ExplorerPatcher", L"Start_ShowClassicMode",
                             0) == 1)
    {
        return 1;
    }
    return 0;
}

int kt_env_tile_notifications(void)
{
    if (kt_windows_build() >= 17763)
    {
        return 1;
    }
    return 0;
}

int kt_env_probe(char* buf, int cch)
{
    int build;
    int n;
    if (!buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    build = kt_windows_build();
    n = sprintf(buf, "{\"explorerPatcher\":%d,\"win10Start\":%d,\"tileNotifications\":%d,\"build\":%d}",
                kt_env_explorerpatcher(), kt_env_win10_start(), kt_env_tile_notifications(), build);
    if (n < 0 || n + 1 > cch)
    {
        buf[0] = '\0';
        kt_err("buffer too small");
        return KT_ERR;
    }
    return KT_OK;
}
