/**
 * @file device_manager.c
 * @brief 多模组 manager 实现。
 *
 * 每 2 秒扫描一次：COM 端口（QueryDosDeviceW）+ NCM/RNDIS 网卡（ncm_enumerate）。
 * Diff 后增删 modem_dev_t，触发 on_change 回调。
 */
#include "device_manager.h"
#include "ncm_chan.h"
#include "serial_chan.h"
#include "at_session.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCAN_INTERVAL_MS 500   /* 0.5s 扫描——插拔 COM/NCM 模组更快响应 */
#define MAX_SCAN_RESULT  64

/* === 工具：列 COM 端口 === */

/**
 * @brief 通过 QueryDosDeviceW 列出所有 "COM<n>" 设备名。
 * @return 写入 out 的数量（≤ max）
 */
static int list_com_ports(char out[][8], int max)
{
    const DWORD buf_size = 64 * 1024;
    wchar_t *buf = (wchar_t *)malloc(buf_size);
    if (!buf) return 0;
    DWORD got = QueryDosDeviceW(NULL, buf, buf_size);
    if (got == 0) { free(buf); return 0; }
    int n = 0;
    wchar_t *p = buf;
    while (*p && n < max) {
        if (wcsncmp(p, L"COM", 3) == 0) {
            /* "COM7" 至 "COM99" 长度 4~5；留 8 字节保险 */
            char name[8];
            WideCharToMultiByte(CP_ACP, 0, p, 7, name, sizeof(name), NULL, NULL);
            name[7] = '\0';
            strncpy(out[n], name, 8);
            n++;
        }
        p += wcslen(p) + 1;
    }
    free(buf);
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
    int  com_n = list_com_ports(com_now, MAX_SCAN_RESULT);

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
        snprintf(d->id, sizeof(d->id), "MDM-COM%s", com_now[i]);
        snprintf(d->label, sizeof(d->label), "COM %s", com_now[i]);
        snprintf(d->chan_uri, sizeof(d->chan_uri), "com://%s?baud=115200", com_now[i]);
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

    fprintf(stderr, "device_manager: 扫描 diff — COM +%d/-%d, NCM +%d/-%d → 共 %d 设备\n",
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
    fprintf(stderr, "device_manager: 启动扫描，间隔 %d ms\n", SCAN_INTERVAL_MS);
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
        fprintf(stderr, "device_manager: 暂只支持 com:// 通道（%s）\n", d->chan_uri);
        return AGENT_ERR_BAD_ARG;
    }

    /* 分配 serial_chan + 打开 */
    d->serial = serial_chan_create(m->loop);
    if (!d->serial) return AGENT_ERR_OOM;
    modem_chan_t *chan = &d->serial->chan;
    if (serial_chan_open(chan, d->chan_uri) != 0) {
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_ERROR;
        return AGENT_ERR_IO;
    }

    /* 分配 at_session */
    d->at = at_session_create(m->loop, chan);
    if (!d->at) {
        serial_chan_close(chan);
        free(d->serial);
        d->serial = NULL;
        d->state = DEV_STATE_ERROR;
        return AGENT_ERR_OOM;
    }
    at_session_open(d->at);

    d->state = DEV_STATE_READY;
    fprintf(stderr, "device_manager: dev %d (%s) 已连接\n", dev_idx, d->label);
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
    fprintf(stderr, "device_manager: dev %d (%s) 已断开\n", dev_idx, d->label);
    return 0;
}
