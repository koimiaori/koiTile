#include "koitile_internal.h"

static int kt_com_begin(int* uninit)
{
    HRESULT hr;
    *uninit = 0;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        *uninit = 1;
        return KT_OK;
    }
    if (hr == RPC_E_CHANGED_MODE)
    {
        return KT_OK;
    }
    kt_err("COM initialization failed");
    return KT_ERR;
}

static void kt_com_end(int uninit)
{
    if (uninit)
    {
        CoUninitialize();
    }
}

static int kt_set_aumid(IPropertyStore* store, const char* aumid)
{
    PROPVARIANT pv;
    wchar_t* w;
    HRESULT hr;
    PropVariantInit(&pv);
    if (!aumid || !aumid[0])
    {
        pv.vt = VT_EMPTY;
        hr = IPropertyStore_SetValue(store, &PKEY_AppUserModel_ID, &pv);
        PropVariantClear(&pv);
        return SUCCEEDED(hr) ? KT_OK : KT_ERR;
    }
    w = kt_utf8_to_utf16(aumid);
    if (!w)
    {
        kt_err("invalid aumid");
        return KT_ERR;
    }
    pv.vt = VT_LPWSTR;
    pv.pwszVal = w;
    hr = IPropertyStore_SetValue(store, &PKEY_AppUserModel_ID, &pv);
    pv.vt = VT_EMPTY;
    PropVariantClear(&pv);
    free(w);
    return SUCCEEDED(hr) ? KT_OK : KT_ERR;
}

static int kt_target_is_path(const char* target)
{
    int i;
    if (!target || !target[0])
    {
        return 0;
    }
    if ((target[0] >= 'a' && target[0] <= 'z') || (target[0] >= 'A' && target[0] <= 'Z'))
    {
        if (target[1] == ':' && (target[2] == '\\' || target[2] == '/'))
        {
            return 1;
        }
    }
    for (i = 0; target[i]; i++)
    {
        if (target[i] == ':')
        {
            return 0;
        }
        if (target[i] == '\\' || target[i] == '/')
        {
            return 1;
        }
    }
    return 1;
}

int kt_shortcut_create(const char* target, const char* link, const char* aumid, const char* args,
                       const char* work_dir)
{
    wchar_t* wlink = NULL;
    wchar_t* wtarget = NULL;
    wchar_t* wargs = NULL;
    wchar_t* wwork = NULL;
    char dir[KT_MAX_PATHBUF];
    IShellLinkW* shell_link = NULL;
    IPersistFile* persist = NULL;
    IPropertyStore* store = NULL;
    HRESULT hr;
    int uninit = 0;
    int rc = KT_ERR;
    int mode_full;
    if (!link || !link[0])
    {
        kt_err("link is empty");
        return KT_ERR;
    }
    mode_full = (target && target[0]) ? 1 : 0;
    if (kt_path_dirname(link, dir, KT_MAX_PATHBUF) != KT_OK)
    {
        return KT_ERR;
    }
    if (kt_dir_create(dir) != KT_OK)
    {
        return KT_ERR;
    }
    if (mode_full && kt_target_is_path(target) && !kt_file_exists(target))
    {
        kt_err("shortcut target not found");
        return KT_ERR;
    }
    if (!mode_full && !kt_file_exists(link))
    {
        kt_err("shortcut not found");
        return KT_ERR;
    }
    if (kt_com_begin(&uninit) != KT_OK)
    {
        return KT_ERR;
    }
    wlink = kt_utf8_to_utf16(link);
    if (!wlink)
    {
        kt_err("invalid link path");
        goto done;
    }
    if (mode_full)
    {
        wtarget = kt_utf8_to_utf16(target);
        wargs = kt_utf8_to_utf16(args ? args : "");
        if (work_dir && work_dir[0])
        {
            wwork = kt_utf8_to_utf16(work_dir);
        }
        else
        {
            wwork = kt_utf8_to_utf16(dir);
        }
        if (!wtarget || !wargs || !wwork)
        {
            kt_err("invalid shortcut field");
            goto done;
        }
    }
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW,
                          (void**)&shell_link);
    if (FAILED(hr) || !shell_link)
    {
        kt_err("create shell link failed");
        goto done;
    }
    if (mode_full)
    {
        if (FAILED(IShellLinkW_SetPath(shell_link, wtarget)))
        {
            kt_err("set shortcut target failed");
            goto done;
        }
        if (FAILED(IShellLinkW_SetArguments(shell_link, wargs)))
        {
            kt_err("set shortcut arguments failed");
            goto done;
        }
        if (FAILED(IShellLinkW_SetWorkingDirectory(shell_link, wwork)))
        {
            kt_err("set shortcut workdir failed");
            goto done;
        }
        IShellLinkW_SetDescription(shell_link, wtarget);
    }
    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPersistFile, (void**)&persist);
    if (FAILED(hr) || !persist)
    {
        kt_err("shell link persist unavailable");
        goto done;
    }
    if (!mode_full)
    {
        if (FAILED(IPersistFile_Load(persist, wlink, STGM_READWRITE)))
        {
            kt_err("open shortcut failed");
            goto done;
        }
    }
    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPropertyStore, (void**)&store);
    if (FAILED(hr) || !store)
    {
        kt_err("property store unavailable");
        goto done;
    }
    if (kt_set_aumid(store, aumid) != KT_OK)
    {
        kt_err("set aumid failed");
        goto done;
    }
    if (FAILED(IPropertyStore_Commit(store)))
    {
        kt_err("commit property store failed");
        goto done;
    }
    if (FAILED(IPersistFile_Save(persist, wlink, TRUE)))
    {
        kt_err("save shortcut failed");
        goto done;
    }
    rc = KT_OK;
done:
    if (store)
    {
        IPropertyStore_Release(store);
    }
    if (persist)
    {
        IPersistFile_Release(persist);
    }
    if (shell_link)
    {
        IShellLinkW_Release(shell_link);
    }
    free(wlink);
    free(wtarget);
    free(wargs);
    free(wwork);
    kt_com_end(uninit);
    return rc;
}

int kt_shortcut_aumid(const char* link, char* buf, int cch)
{
    wchar_t* wlink = NULL;
    IShellLinkW* shell_link = NULL;
    IPersistFile* persist = NULL;
    IPropertyStore* store = NULL;
    PROPVARIANT pv;
    HRESULT hr;
    int uninit = 0;
    int rc = KT_ERR;
    if (!link || !link[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    PropVariantInit(&pv);
    if (kt_com_begin(&uninit) != KT_OK)
    {
        return KT_ERR;
    }
    wlink = kt_utf8_to_utf16(link);
    if (!wlink)
    {
        kt_err("invalid link path");
        goto done;
    }
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW,
                          (void**)&shell_link);
    if (FAILED(hr) || !shell_link)
    {
        kt_err("create shell link failed");
        goto done;
    }
    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPersistFile, (void**)&persist);
    if (FAILED(hr) || !persist)
    {
        kt_err("shell link persist unavailable");
        goto done;
    }
    if (FAILED(IPersistFile_Load(persist, wlink, STGM_READ)))
    {
        kt_err("open shortcut failed");
        goto done;
    }
    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPropertyStore, (void**)&store);
    if (FAILED(hr) || !store)
    {
        kt_err("property store unavailable");
        goto done;
    }
    if (FAILED(IPropertyStore_GetValue(store, &PKEY_AppUserModel_ID, &pv)))
    {
        rc = KT_OK;
        goto done;
    }
    if (pv.vt == VT_LPWSTR && pv.pwszVal)
    {
        if (kt_utf16_to_utf8_buf(pv.pwszVal, (int)wcslen(pv.pwszVal), buf, cch) != KT_OK)
        {
            goto done;
        }
    }
    rc = KT_OK;
done:
    PropVariantClear(&pv);
    if (store)
    {
        IPropertyStore_Release(store);
    }
    if (persist)
    {
        IPersistFile_Release(persist);
    }
    if (shell_link)
    {
        IShellLinkW_Release(shell_link);
    }
    free(wlink);
    kt_com_end(uninit);
    return rc;
}

static int kt_shortcut_field(const char* link, int field, char* buf, int cch)
{
    wchar_t* wlink = NULL;
    wchar_t wvalue[KT_MAX_PATHBUF];
    IShellLinkW* shell_link = NULL;
    IPersistFile* persist = NULL;
    HRESULT hr;
    int uninit = 0;
    int rc = KT_ERR;
    if (!link || !link[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    wvalue[0] = L'\0';
    if (kt_com_begin(&uninit) != KT_OK)
    {
        return KT_ERR;
    }
    wlink = kt_utf8_to_utf16(link);
    if (!wlink)
    {
        kt_err("invalid link path");
        goto done;
    }
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW,
                          (void**)&shell_link);
    if (FAILED(hr) || !shell_link)
    {
        kt_err("create shell link failed");
        goto done;
    }
    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPersistFile, (void**)&persist);
    if (FAILED(hr) || !persist)
    {
        kt_err("shell link persist unavailable");
        goto done;
    }
    if (FAILED(IPersistFile_Load(persist, wlink, STGM_READ)))
    {
        kt_err("open shortcut failed");
        goto done;
    }
    if (field == 0)
    {
        if (FAILED(IShellLinkW_GetPath(shell_link, wvalue, KT_MAX_PATHBUF, NULL, SLGP_RAWPATH)))
        {
            kt_err("read shortcut target failed");
            goto done;
        }
    }
    else if (field == 1)
    {
        if (FAILED(IShellLinkW_GetArguments(shell_link, wvalue, KT_MAX_PATHBUF)))
        {
            kt_err("read shortcut arguments failed");
            goto done;
        }
    }
    else
    {
        if (FAILED(IShellLinkW_GetWorkingDirectory(shell_link, wvalue, KT_MAX_PATHBUF)))
        {
            kt_err("read shortcut workdir failed");
            goto done;
        }
    }
    if (kt_utf16_to_utf8_buf(wvalue, (int)wcslen(wvalue), buf, cch) != KT_OK)
    {
        goto done;
    }
    rc = KT_OK;
done:
    if (persist)
    {
        IPersistFile_Release(persist);
    }
    if (shell_link)
    {
        IShellLinkW_Release(shell_link);
    }
    free(wlink);
    kt_com_end(uninit);
    return rc;
}

int kt_shortcut_target_of(const char* link, char* buf, int cch)
{
    if (!link || !link[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    return kt_shortcut_target(link, buf, cch);
}
int kt_shortcut_target(const char* link, char* buf, int cch)
{
    return kt_shortcut_field(link, 0, buf, cch);
}

int kt_shortcut_args(const char* link, char* buf, int cch)
{
    return kt_shortcut_field(link, 1, buf, cch);
}

int kt_shortcut_work_dir(const char* link, char* buf, int cch)
{
    return kt_shortcut_field(link, 2, buf, cch);
}

int kt_shell_refresh(void)
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return KT_OK;
}

int kt_shell_notify(const char* path)
{
    wchar_t* w;
    DWORD attr;
    if (!path || !path[0])
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    w = kt_utf8_to_utf16(path);
    if (!w)
    {
        kt_err("invalid path");
        return KT_ERR;
    }
    attr = GetFileAttributesW(w);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, w, NULL);
    }
    else
    {
        SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, w, NULL);
    }
    free(w);
    return KT_OK;
}
