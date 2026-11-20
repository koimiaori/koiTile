#include "koitile_internal.h"

static int kt_http_convert_body(char* raw, DWORD got, char* buf, int cch)
{
    DWORD start = 0;
    DWORD i;
    DWORD len;
    if (got >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF)
    {
        start = 3;
    }
    else if (got >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE)
    {
        return kt_utf16_to_utf8_buf((const wchar_t*)(const void*)(raw + 2), (int)((got - 2) / 2), buf,
                                    cch);
    }
    else if (got >= 2 && (unsigned char)raw[0] == 0xFE && (unsigned char)raw[1] == 0xFF)
    {
        int count = (int)((got - 2) / 2);
        wchar_t* tmp;
        int rc;
        if (count <= 0)
        {
            buf[0] = '\0';
            return KT_OK;
        }
        tmp = (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t));
        if (!tmp)
        {
            kt_err("out of memory");
            return KT_ERR;
        }
        for (i = 0; i < (DWORD)count; i++)
        {
            tmp[i] = (wchar_t)(((unsigned char)raw[2 + i * 2] << 8) |
                               (unsigned char)raw[2 + i * 2 + 1]);
        }
        tmp[count] = L'\0';
        rc = kt_utf16_to_utf8_buf(tmp, count, buf, cch);
        free(tmp);
        return rc;
    }
    len = got - start;
    if ((int)len + 1 > cch)
    {
        kt_err("buffer too small");
        return KT_ERR;
    }
    if (len > 0)
    {
        memmove(buf, raw + start, len);
    }
    buf[len] = '\0';
    return KT_OK;
}

int kt_http_get(const char* url, char* buf, int cch, int timeout_ms)
{
    wchar_t* wurl = NULL;
    HINTERNET session = NULL;
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    char* raw = NULL;
    DWORD got = 0;
    DWORD total = 0;
    DWORD available = 0;
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    int timeout;
    int rc = KT_ERR;
    URL_COMPONENTS parts;
    wchar_t host[256];
    wchar_t path[2048];
    wchar_t scheme[16];
    INTERNET_PORT port;
    if (!url || !url[0] || !buf || cch <= 0)
    {
        kt_err("invalid argument");
        return KT_ERR;
    }
    buf[0] = '\0';
    timeout = timeout_ms > 0 ? timeout_ms : 15000;
    raw = (char*)malloc(KT_MAX_HTTP + 2);
    if (!raw)
    {
        kt_err("out of memory");
        return KT_ERR;
    }
    wurl = kt_utf8_to_utf16(url);
    if (!wurl)
    {
        kt_err("invalid url");
        goto done;
    }
    memset(&parts, 0, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = 255;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2047;
    parts.lpszScheme = scheme;
    parts.dwSchemeLength = 15;
    if (!WinHttpCrackUrl(wurl, 0, 0, &parts))
    {
        kt_err("invalid url");
        goto done;
    }
    if (parts.nPort != 0)
    {
        port = parts.nPort;
    }
    else
    {
        port = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT
                                                        : INTERNET_DEFAULT_HTTP_PORT;
    }
    session = WinHttpOpen(L"koiTile/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        kt_err("winhttp open failed");
        goto done;
    }
    WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
    connection = WinHttpConnect(session, host, port, 0);
    if (!connection)
    {
        kt_err("winhttp connect failed");
        goto done;
    }
    request = WinHttpOpenRequest(connection, L"GET", path[0] ? path : L"/", NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 (parts.nScheme == INTERNET_SCHEME_HTTPS)
                                     ? WINHTTP_FLAG_SECURE
                                     : 0);
    if (!request)
    {
        kt_err("winhttp request failed");
        goto done;
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                            0, 0))
    {
        kt_err("winhttp send failed");
        goto done;
    }
    if (!WinHttpReceiveResponse(request, NULL))
    {
        kt_err("winhttp receive failed");
        goto done;
    }
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX))
    {
        kt_err("winhttp status failed");
        goto done;
    }
    if (status < 200 || status >= 300)
    {
        kt_err("http request failed");
        goto done;
    }
    while (1)
    {
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            kt_err("winhttp read failed");
            goto done;
        }
        if (available == 0)
        {
            break;
        }
        if (total + available > (DWORD)KT_MAX_HTTP)
        {
            kt_err("response too large");
            goto done;
        }
        if (!WinHttpReadData(request, raw + total, available, &got))
        {
            kt_err("winhttp read failed");
            goto done;
        }
        if (got == 0)
        {
            break;
        }
        total += got;
    }
    raw[total] = '\0';
    raw[total + 1] = '\0';
    rc = kt_http_convert_body(raw, total, buf, cch);
done:
    if (request)
    {
        WinHttpCloseHandle(request);
    }
    if (connection)
    {
        WinHttpCloseHandle(connection);
    }
    if (session)
    {
        WinHttpCloseHandle(session);
    }
    free(raw);
    free(wurl);
    return rc;
}
