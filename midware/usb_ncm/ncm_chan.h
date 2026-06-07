/**
 * @file ncm_chan.h
 * @brief USB-NCM / RNDIS 网卡枚举（IP Helper）。
 *
 * P2 只做"发现 + 报告"，不做数据面（数据面留给 P3）。
 */
#ifndef MIDWARE_NCM_CHAN_H
#define MIDWARE_NCM_CHAN_H

#include <stdbool.h>
#include <stddef.h>

#define NCM_MAX_INTERFACES 16

typedef struct {
    char  if_name[128];     /* 友好名 "Mobile broadband adapter" */
    char  adapter_name[64]; /* "以太网 X" */
    char  ipv4[16];         /* "10.42.0.7"，空字符串 = 无 IPv4 */
    bool  is_candidate;     /* 描述符匹配 RNDIS/NCM/CDC-ECM */
} ncm_interface_t;

/**
 * @brief 枚举所有 USB-NCM / RNDIS / CDC-ECM 候选网卡。
 * @param out  输出数组
 * @param max  数组容量
 * @return 实际写入数量（≤ max）；0 表示没找到或出错
 */
int ncm_enumerate(ncm_interface_t *out, int max);

#endif
