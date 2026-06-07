/**
 * @file ncm_chan.c
 * @brief NCM/RNDIS 网卡枚举：GetAdaptersAddresses 匹配关键字。
 *
 * 关键约束：winsock2.h 必须在 iphlpapi.h 之前 include；inet_ntop 需要 ws2tcpip.h。
 */
#include "ncm_chan.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>       /* inet_ntop, INET_ADDRSTRLEN */
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief 描述符关键字匹配（大小写敏感，Win 描述符里关键字大小写固定）。
 */
static bool description_matches_ncm(const char *desc)
{
    if (!desc) return false;
    static const char *kKeywords[] = {
        "Remote NDIS", "RNDIS", "NCM", "CDC-ECM", "CDC NCM",
        "Mobile Broadband", "MBIM", "wwan", "WWAN", NULL
    };
    for (int i = 0; kKeywords[i]; i++) {
        if (strstr(desc, kKeywords[i])) return true;
    }
    return false;
}

/**
 * @brief 把 wchar_t* 友好名转 UTF-8（避免引额外的 <wchar.h>）。
 */
static void wcs_to_utf8(const wchar_t *src, char *dst, size_t dst_len)
{
    if (!src || !dst || dst_len == 0) { if (dst_len) dst[0] = '\0'; return; }
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dst_len, NULL, NULL);
}

int ncm_enumerate(ncm_interface_t *out, int max)
{
    if (!out || max <= 0) return 0;

    ULONG buf_len = 16 * 1024;
    IP_ADAPTER_ADDRESSES *buf = (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
    if (!buf) return 0;

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER;
    ULONG rc = GetAdaptersAddresses(AF_INET, flags, NULL, buf, &buf_len);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        IP_ADAPTER_ADDRESSES *new_buf = (IP_ADAPTER_ADDRESSES *)realloc(buf, buf_len);
        if (!new_buf) { free(buf); return 0; }
        buf = new_buf;
        rc = GetAdaptersAddresses(AF_INET, flags, NULL, buf, &buf_len);
    }
    if (rc != NO_ERROR) {
        fprintf(stderr, "ncm_enumerate: GetAdaptersAddresses failed rc=%lu\n", rc);
        free(buf);
        return 0;
    }

    int n = 0;
    for (IP_ADAPTER_ADDRESSES *a = buf; a && n < max; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        /* IP_ADAPTER_ADDRESSES::Description 是 PWCHAR（UTF-16），先转 UTF-8 再匹配 */
        char desc_utf8[256] = {0};
        if (a->Description) {
            wcs_to_utf8(a->Description, desc_utf8, sizeof(desc_utf8));
        }
        if (!description_matches_ncm(desc_utf8)) continue;

        ncm_interface_t *dst = &out[n++];
        memset(dst, 0, sizeof(*dst));
        if (a->FriendlyName) {
            wcs_to_utf8(a->FriendlyName, dst->if_name, sizeof(dst->if_name));
        }
        if (a->AdapterName) {
            strncpy(dst->adapter_name, a->AdapterName, sizeof(dst->adapter_name) - 1);
        }
        /* 取第一个 IPv4 */
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)u->Address.lpSockaddr;
                inet_ntop(AF_INET, &sa->sin_addr, dst->ipv4, sizeof(dst->ipv4));
                break;
            }
        }
        dst->is_candidate = true;
        fprintf(stderr, "ncm_enumerate: hit '%s' ipv4=%s\n",
                dst->if_name, dst->ipv4[0] ? dst->ipv4 : "(none)");
    }

    free(buf);
    return n;
}
