/**
 * @file device_manager.c
 * @brief 多模组 manager 实现。
 *
 * 每 0.5 秒扫描一次：
 *   1) QueryDosDeviceW 列所有 COM 端口（保证能列出来，Win32 老 API）
 *   2) 对每个 COM 端口，SetupDi+GUID_DEVCLASS_PORTS 反向查友好名（不一定能查到，失败用 "COM <n>" 兜底）
 *   3) NCM/RNDIS 网卡（ncm_enumerate）
 *
 * Diff 后增删 modem_dev_t，触发 on_change 回调。
 *
 * 两阶段设计：CDC-ACM 模组可能注册在非标准设备类里，纯 SetupDi 枚举会漏；
 * 改用 QueryDosDeviceW 兜底，SetupDi 只用来拿友好名，失败也无妨。
 */
#include "device_manager.h"
#include "ncm_chan.h"
#include "serial_chan.h"
#include "at_session.h"

/**
 * @brief 默认串口波特率（跨模块共享，定义在 core/main.cpp）。
 *
 * 通过 extern 引入，避免在公共头 agent_types.h 加字段污染所有模块。
 * P3 简化：用户改 baud 后，只对**新**扫描出来的 dev 生效；已存在的 dev
 * 要等下次扫描重建（插拔或 0.5s tick 检测到变化）才用新 baud。
 * P3.5 计划：挪到 agent_app_t 字段上。
 */
extern int g_default_baud;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uv.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")  /* MSVC 提示；MinGW 用 target_link_libraries */

/**
 * @brief 端口(COM & LPT)设备类 GUID（手写避免 MinGW/clang 下 DEFINE_GUID 链接 COMDAT 问题）。
 *
 * 官方值 GUID_DEVCLASS_PORTS = {86E0D1E0-8089-11D0-9CE4-08003E301F73}。
 * 见 MSDN: System-Defined Device Setup Classes → Ports (COM & LPT)。
 * MinGW 的 <devguid.h> 用 DEFINE_GUID 声明，但 selectany COMDAT 在
 * clang-lld 下链接期找不到——所以这里直接 static const 展开。
 */
static const GUID kGuidClassPorts = {
    0x86E0D1E0, 0x8089, 0x11D0,
    { 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73 }
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCAN_INTERVAL_MS 500   /* 0.5s 扫描——插拔 COM/NCM 模组更快响应 */
#define MAX_SCAN_RESULT  64

/* === 工具：列 COM 端口 === */

/**
 * @brief 通过 QueryDosDeviceW 列出所有 COM 端口，附加 SetupDi 查到的友好名。
 *
 * 两阶段：
 *   1. QueryDosDeviceW 列所有 "COM<n>" 设备（保证能列出来，Win32 老 API）
 *   2. 对每个 COM 端口，SetupDi 反向查友好名（不一定能查到，失败用 "COM <n>" 兜底）
 *
 * P3 暂不按设备类过滤——CDC-ACM 模组可能注册在任意设备类里，SetupDi 枚举
 * 不一定能找到；不如让用户看到所有端口并通过友好名识别。
 *
 * @param out_com  输出纯数字端口号数组（"11" 等，已剥掉 "COM" 前缀），最多 max 项
 * @param out_name 输出 label 数组（优先友好名，回退 "COM <n>"），最多 max 项
 * @param max     数组容量
 * @return 实际写入数量
 */
static int list_com_ports_with_names(char out_com[][8], char out_name[][128], int max)
{
    /* 第一阶段：QueryDosDeviceW 列所有 COM 端口 */
    const DWORD buf_size = 64 * 1024;
    wchar_t *buf = (wchar_t *)malloc(buf_size);
    if (!buf) return 0;
    DWORD got = QueryDosDeviceW(NULL, buf, buf_size);
    if (got == 0) {
        fprintf(stderr, "device_manager: QueryDosDeviceW failed: %lu\n", GetLastError());
        free(buf);
        return 0;
    }
    int n = 0;
    wchar_t *p = buf;
    while (*p && n < max) {
        if (wcsncmp(p, L"COM", 3) == 0) {
            char name[8];
            WideCharToMultiByte(CP_ACP, 0, p, 7, name, sizeof(name), NULL, NULL);
            name[7] = '\0';
            /* 剥掉前缀 "COM" 只留数字（"COM11" → "11"），方便 label/id 拼接 */
            const char *digits = name;
            if (digits[0] == 'C' && digits[1] == 'O' && digits[2] == 'M') {
                digits += 3;
            }
            strncpy(out_com[n], digits, 8);
            n++;
        }
        p += wcslen(p) + 1;
    }
    free(buf);

    /* 第二阶段：给每个 COM 端口反查友好名 */
    HDEVINFO dev_info = SetupDiGetClassDevsW(&kGuidClassPorts, NULL, NULL, DIGCF_PRESENT);
    if (dev_info == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "device_manager: SetupDiGetClassDevs failed (friendly-name only): %lu\n",
                GetLastError());
        /* SetupDi 失败：所有端口用兜底名 "COM <n>" */
        for (int i = 0; i < n; i++) {
            snprintf(out_name[i], 128, "COM %s", out_com[i]);
            fprintf(stderr, "device_manager: port %s: '%s' (fallback)\n",
                    out_com[i], out_name[i]);
        }
        return n;
    }
    SP_DEVINFO_DATA dev_info_data;
    dev_info_data.cbSize = sizeof(dev_info_data);

    for (int i = 0; i < n; i++) {
        out_name[i][0] = '\0';
        for (DWORD j = 0; SetupDiEnumDeviceInfo(dev_info, j, &dev_info_data); j++) {
            HKEY hKey = SetupDiOpenDevRegKey(dev_info, &dev_info_data,
                                            DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
            if (hKey == INVALID_HANDLE_VALUE) continue;

            wchar_t portName[32] = {0};
            DWORD len = sizeof(portName);
            LONG rc = RegQueryValueExW(hKey, L"PortName", NULL, NULL,
                                      (LPBYTE)portName, &len);
            RegCloseKey(hKey);
            if (rc != ERROR_SUCCESS || portName[0] == '\0') continue;

            /* 拿掉 "COM" 前缀得到纯端口名 */
            const wchar_t *q = portName;
            if (wcsncmp(q, L"COM", 3) == 0) q += 3;
            else continue;

            char com_name[8];
            WideCharToMultiByte(CP_ACP, 0, q, 7, com_name, sizeof(com_name), NULL, NULL);
            com_name[7] = '\0';
            if (strcmp(com_name, out_com[i]) != 0) continue;

            /* 匹配：拿友好名 */
            wchar_t friendly[256] = {0};
            SetupDiGetDeviceRegistryPropertyW(dev_info, &dev_info_data,
                SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendly, sizeof(friendly), NULL);
            char aname[128];
            WideCharToMultiByte(CP_UTF8, 0, friendly, 127,
                                aname, sizeof(aname) - 1, NULL, NULL);
            aname[sizeof(aname) - 1] = '\0';
            /* 友好名可能自带 "(COMn)"，截到 ( 之前以避免重复 */
            char *p_paren = strchr(aname, '(');
            if (p_paren && p_paren > aname) *(p_paren - 1) = '\0';
            if (aname[0] == '\0') snprintf(aname, sizeof(aname), "COM %s", out_com[i]);
            strncpy(out_name[i], aname, 128);
            break;  /* 找到匹配就跳出内层循环 */
        }
        if (out_name[i][0] == '\0') {
            snprintf(out_name[i], 128, "COM %s", out_com[i]);
        }
        fprintf(stderr, "device_manager: port %s: '%s'\n", out_com[i], out_name[i]);
    }
    SetupDiDestroyDeviceInfoList(dev_info);
    return n;
}

/* === diff helper === */

static int find_serial(const char (*arr)[8], int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i], name) == 0) return i;
    }
    return -1;
}

static int find_ncm(const char (*arr)[128], int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i], name) == 0) return i;
    }
    return -1;
}

/* === 主扫描（由 libuv timer 触发） === */

static void post_change_event(device_manager_t *m)
{
    if (!m->on_change) return;
    dev_list_snapshot_t snap;
    memcpy(snap.devs, m->devs, sizeof(m->devs));
    snap.count = m->dev_count;
    m->on_change(m->userdata, &snap);
}

static void do_scan(uv_timer_t *handle)
{
    device_manager_t *m = (device_manager_t *)handle->data;
    if (!m) return;

    char com_now[MAX_SCAN_RESULT][8];
    char com_name_now[MAX_SCAN_RESULT][128];
    int  com_n = list_com_ports_with_names(com_now, com_name_now, MAX_SCAN_RESULT);

    ncm_interface_t ncm_now[NCM_MAX_INTERFACES];
    int  ncm_n = ncm_enumerate(ncm_now, NCM_MAX_INTERFACES);

    /* diff COM */
    int com_added = 0, com_removed = 0;
    for (int i = 0; i < com_n; i++) {
        if (find_serial(m->prev_serial, m->prev_serial_count, com_now[i]) < 0) com_added++;
    }
    for (int i = 0; i < m->prev_serial_count; i++) {
        if (find_serial(com_now, com_n, m->prev_serial[i]) < 0) com_removed++;
    }
    /* diff NCM */
    int ncm_added = 0, ncm_removed = 0;
    /* 临时把 ncm_now 投影成 if_name 字符串数组，便于复用 find_ncm */
    char ncm_names[NCM_MAX_INTERFACES][128];
    for (int i = 0; i < ncm_n; i++) {
        strncpy(ncm_names[i], ncm_now[i].if_name, 128);
    }
    for (int i = 0; i < ncm_n; i++) {
        if (find_ncm(m->prev_ncm, m->prev_ncm_count, ncm_now[i].if_name) < 0) ncm_added++;
    }
    for (int i = 0; i < m->prev_ncm_count; i++) {
        if (find_ncm(ncm_names, ncm_n, m->prev_ncm[i]) < 0) ncm_removed++;
    }

    if (com_added == 0 && com_removed == 0 && ncm_added == 0 && ncm_removed == 0) {
        return;  /* 无变化，跳过重建 */
    }

    /* 重建 m->devs */
    m->dev_count = 0;
    for (int i = 0; i < com_n && m->dev_count < DEV_MANAGER_MAX_DEVS; i++) {
        modem_dev_t *d = &m->devs[m->dev_count++];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "MDM-COM%s", com_now[i]);  /* com_now[i] 现在是 "11" → "MDM-COM11" */
        /* label 优先用友好名（"Quectel Mobile Broadband Modem" 等），回退到 "COM 11"（注意空格 + 数字） */
        if (com_name_now[i][0] != '\0') {
            strncpy(d->label, com_name_now[i], sizeof(d->label) - 1);
        } else {
            snprintf(d->label, sizeof(d->label), "COM %s", com_now[i]);
        }
        /* chan_uri 需要完整 "COM<n>" 形式，com:// + "COM" + 数字；
         * baud 用 g_default_baud（用户在 settings panel 选 9600 / 115200 等），
         * 不再写死 115200——用户模组真机是 9600，写死会导致 AT 命令解不出。 */
        snprintf(d->chan_uri, sizeof(d->chan_uri), "com://COM%s?baud=%d",
                 com_now[i], g_default_baud);
        d->state = DEV_STATE_DISCONNECTED;
    }
    for (int i = 0; i < ncm_n && m->dev_count < DEV_MANAGER_MAX_DEVS; i++) {
        modem_dev_t *d = &m->devs[m->dev_count++];
        memset(d, 0, sizeof(*d));
        snprintf(d->id, sizeof(d->id), "MDM-NCM%d", i);
        snprintf(d->label, sizeof(d->label), "%s", ncm_now[i].if_name);
        snprintf(d->chan_uri, sizeof(d->chan_uri), "rndis://%s", ncm_now[i].if_name);
        strncpy(d->ipv4, ncm_now[i].ipv4, sizeof(d->ipv4) - 1);
        d->state = DEV_STATE_DISCONNECTED;
    }

    /* 更新 prev 快照 */
    for (int i = 0; i < com_n; i++) strncpy(m->prev_serial[i], com_now[i], 8);
    m->prev_serial_count = com_n;
    for (int i = 0; i < ncm_n; i++) strncpy(m->prev_ncm[i], ncm_now[i].if_name, 128);
    m->prev_ncm_count = ncm_n;

    fprintf(stderr, "device_manager: scan diff -- COM +%d/-%d, NCM +%d/-%d -> %d devices total\n",
            com_added, com_removed, ncm_added, ncm_removed, m->dev_count);
    post_change_event(m);
}

int device_manager_init(device_manager_t *m, uv_loop_t *loop)
{
    if (!m || !loop) return AGENT_ERR_BAD_ARG;
    memset(m, 0, sizeof(*m));
    m->loop = loop;
    m->scan_timer = (uv_timer_t *)calloc(1, sizeof(uv_timer_t));
    if (!m->scan_timer) return AGENT_ERR_OOM;
    return AGENT_OK;
}

int device_manager_start(device_manager_t *m)
{
    if (!m || !m->loop || !m->scan_timer) return AGENT_ERR_BAD_ARG;
    int r = uv_timer_init(m->loop, m->scan_timer);
    if (r != 0) return AGENT_ERR_IO;
    m->scan_timer->data = m;
    r = uv_timer_start(m->scan_timer, do_scan, 0, SCAN_INTERVAL_MS);
    if (r != 0) return AGENT_ERR_IO;
    fprintf(stderr, "device_manager: scan started, interval %d ms\n", SCAN_INTERVAL_MS);
    return AGENT_OK;
}

void device_manager_stop(device_manager_t *m)
{
    if (!m || !m->scan_timer) return;
    uv_timer_stop(m->scan_timer);
    uv_close((uv_handle_t *)m->scan_timer, NULL);
}

int device_manager_force_scan(device_manager_t *m)
{
    if (!m || !m->scan_timer) return AGENT_ERR_BAD_ARG;
    do_scan(m->scan_timer);
    return AGENT_OK;
}

void device_manager_set_callback(device_manager_t *m, dev_change_fn fn, void *userdata)
{
    if (!m) return;
    m->on_change = fn;
    m->userdata = userdata;
}

/**
 * @brief 把指定设备连上：分配 serial_chan + 打开 + 创建 at_session + 切 READY。
 *
 * P3 暂只支持 com:// 通道；NCM/RNDIS 留给后续。
 *
 * @param m       manager
 * @param dev_idx 设备索引（0..dev_count-1）
 * @return 0 成功；负数见 agent_errstr
 */
int device_manager_connect_dev(device_manager_t *m, int dev_idx)
{
    if (!m || dev_idx < 0 || dev_idx >= m->dev_count) return AGENT_ERR_BAD_ARG;
    modem_dev_t *d = &m->devs[dev_idx];
    if (d->at || d->serial) return AGENT_ERR_BAD_ARG;  /* 已连接 */

    /* 只支持 COM 串口 */
    if (strncmp(d->chan_uri, "com://", 6) != 0) {
        fprintf(stderr, "device_manager: only com:// supported (%s)\n", d->chan_uri);
        return AGENT_ERR_BAD_ARG;
    }

    /* 分配 serial_chan + 打开 */
    d->serial = serial_chan_create(m->loop);
    if (!d->serial) return AGENT_ERR_OOM;
    modem_chan_t *chan = &d->serial->chan;
    if (serial_chan_open(chan, d->chan_uri) != 0) {
        fprintf(stderr, "device_manager: dev %d (%s) open failed -- back to DISCONNECTED, user can retry\n",
                dev_idx, d->label);
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_DISCONNECTED;
        return AGENT_ERR_IO;
    }

    /* 分配 at_session */
    d->at = at_session_create(m->loop, chan);
    if (!d->at) {
        serial_chan_close(chan);
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_DISCONNECTED;
        return AGENT_ERR_OOM;
    }
    at_session_open(d->at);
    /* P6: 把设备 ID 喂给 at_session，让收发字节落 at_log 时带 device_id。
     * 必须在 at_session_open 之后、首次收发前调一次。 */
    at_session_set_device_id(d->at, d->id);

    d->state = DEV_STATE_READY;
    fprintf(stderr, "device_manager: dev %d (%s) connected\n", dev_idx, d->label);
    return 0;
}

/**
 * @brief 断开指定设备：关 at_session、关串口、释放内存、切回 DISCONNECTED。
 */
int device_manager_disconnect_dev(device_manager_t *m, int dev_idx)
{
    if (!m || dev_idx < 0 || dev_idx >= m->dev_count) return AGENT_ERR_BAD_ARG;
    modem_dev_t *d = &m->devs[dev_idx];
    if (!d->at && !d->serial) return AGENT_ERR_BAD_ARG;

    if (d->at) {
        at_session_close(d->at);
        free(d->at);
        d->at = NULL;
    }
    if (d->serial) {
        serial_chan_close(&d->serial->chan);
        free(d->serial);
        d->serial = NULL;
    }
    d->state = DEV_STATE_DISCONNECTED;
    fprintf(stderr, "device_manager: dev %d (%s) disconnected\n", dev_idx, d->label);
    return 0;
}
