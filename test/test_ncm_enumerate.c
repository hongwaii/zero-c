/**
 * @file test_ncm_enumerate.c
 * @brief 打印当前机器的 RNDIS/NCM/CDC-ECM 网卡。
 * 0 命中也 pass——开发机可能没插模组。
 */
#include "ncm_chan.h"
#include <stdio.h>

int main(void)
{
    ncm_interface_t ifs[NCM_MAX_INTERFACES];
    int n = ncm_enumerate(ifs, NCM_MAX_INTERFACES);
    printf("test_ncm_enumerate: 发现 %d 个 RNDIS/NCM 候选网卡\n", n);
    for (int i = 0; i < n; i++) {
        printf("  [%d] if=%s  adapter=%s  ipv4=%s\n",
               i, ifs[i].if_name, ifs[i].adapter_name, ifs[i].ipv4);
    }
    return 0;
}
