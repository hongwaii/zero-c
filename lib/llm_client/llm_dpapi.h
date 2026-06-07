/**
 * @file llm_dpapi.h
 * @brief Windows DPAPI 加密 / 解密：把 API key 用 CryptProtectData 加密，hex 编码后落盘。
 *
 * 特性：
 *   - DPAPI 范围：本机本用户。换电脑 / 换 Windows 账户无法解密——这是产品需求
 *   - 输出 hex 字符串（每字节 2 字符），便于写到 JSON
 *   - 输入空串 / 缓冲太小返回错误
 */
#ifndef LIB_LLM_DPAPI_H
#define LIB_LLM_DPAPI_H

#include <stdbool.h>
#include <stddef.h>

/* 加密：plaintext -> hex_out（hex 长度 = plaintext_len * 2 + 1）。
 * hex_out_cap 至少 plaintext_len * 2 + 1。返回 0=ok / 负=错误。 */
int  llm_dpapi_encrypt_hex(const char *plaintext, size_t plaintext_len,
                           char *hex_out, size_t hex_out_cap);

/* 解密：hex_in -> plaintext_out。plaintext_out_cap 至少 hex_len / 2。 */
int  llm_dpapi_decrypt_hex(const char *hex_in, size_t hex_len,
                           char *plaintext_out, size_t plaintext_out_cap);

#endif
