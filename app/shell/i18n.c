/**
 * @file i18n.c
 * @brief i18n 字符串加载，支持多路径 fallback（cwd / EXE 目录 / EXE 父目录）。
 */
#include "i18n.h"
#include "json_reader.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* 当前加载的语言根节点；i18n_shutdown 时 delete */
static struct cJSON *g_root = NULL;
static agent_lang_t  g_lang = AGENT_LANG_ZH_CN;

/**
 * @brief 根据语言枚举返回"简短"文件名（zh.json / en.json）。
 */
static const char *basename_for(agent_lang_t l)
{
    switch (l) {
        case AGENT_LANG_EN_US: return "en.json";
        case AGENT_LANG_ZH_CN:
        default:               return "zh.json";
    }
}

/**
 * @brief 拿当前 EXE 文件所在目录（带尾部分隔符）。
 * @param buf 输出 buffer
 * @param buf_len buffer 大小
 * @return 成功写入字节数（含 NUL），失败返回 0
 */
static DWORD get_exe_dir(char *buf, DWORD buf_len)
{
    if (buf_len == 0) return 0;
    DWORD n = GetModuleFileNameA(NULL, buf, buf_len);
    if (n == 0 || n >= buf_len) return 0;
    /* 找到最后一个 \ 或 /，截断到目录 */
    char *p = buf + n;
    while (p > buf && *(p - 1) != '\\' && *(p - 1) != '/') p--;
    *p = '\0';
    return (DWORD)(p - buf);
}

/**
 * @brief 尝试多个候选路径加载语言文件，按顺序返回第一个成功的。
 * @return 加载的 cJSON 根（调用方负责 delete），全部失败返回 NULL。
 */
static struct cJSON *try_load_lang_file(agent_lang_t lang)
{
    const char *base = basename_for(lang);
    char path[1024];
    struct cJSON *root = NULL;

    /* 候选路径（按优先级）。mingw 接受 / 和 \\，统一用 / 更清爽。 */
    const char *cwd_templates[] = {
        "app/i18n/%s",                          /* 1) cwd/app/i18n/（开发/测试） */
        "i18n/%s",                              /* 2) cwd/i18n/（备用） */
    };
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), cwd_templates[i], base);
        if (json_load_file(path, &root) == AGENT_OK) {
            fprintf(stderr, "i18n: loaded %s successfully (cwd-relative)\n", path);
            return root;
        }
    }

    /* 后续候选需要 EXE 目录：3) exe_dir/app/i18n/，4) exe_dir/i18n/，5) exe_dir/../app/i18n/（in-source build 场景） */
    char exe_dir[1024];
    DWORD dn = get_exe_dir(exe_dir, sizeof(exe_dir));
    if (dn == 0) {
        fprintf(stderr, "i18n: GetModuleFileNameA failed\n");
        return NULL;
    }

    const char *exe_templates[] = {
        "app/i18n/%s",
        "i18n/%s",
        "../app/i18n/%s",
    };
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/%s", exe_dir, exe_templates[i]);
        if (json_load_file(path, &root) == AGENT_OK) {
            fprintf(stderr, "i18n: loaded %s successfully (exe_dir-relative)\n", path);
            return root;
        }
    }

    fprintf(stderr, "i18n: %s not found, all candidate paths tried\n", base);
    return NULL;
}

/**
 * @brief 加载语言文件并替换全局根节点。失败时 g_root 保持 NULL。
 */
int i18n_init(agent_lang_t lang)
{
    i18n_shutdown();
    g_root = try_load_lang_file(lang);
    if (!g_root) {
        return AGENT_ERR_NOT_FOUND;
    }
    g_lang = lang;
    return AGENT_OK;
}

/** @brief 释放当前加载的根节点。 */
void i18n_shutdown(void)
{
    if (g_root) { cJSON_Delete(g_root); g_root = NULL; }
    g_lang = AGENT_LANG_ZH_CN;
}

/** @brief 返回当前语言。 */
agent_lang_t i18n_current(void) { return g_lang; }

/**
 * @brief 按 key 取字符串，找不到或类型不匹配时返回 key 本身。
 */
const char *i18n_get(const char *key)
{
    if (!g_root || !key) return key;
    struct cJSON *v = cJSON_GetObjectItemCaseSensitive(g_root, key);
    if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    return key;
}
