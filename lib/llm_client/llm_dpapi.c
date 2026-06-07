/**
 * @file llm_dpapi.c
 */
#include "llm_dpapi.h"
#include "agent_types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include <stdio.h>
#include <string.h>

static const char kHex[] = "0123456789abcdef";

static int to_hex(const uint8_t *src, size_t src_len, char *dst, size_t dst_cap)
{
    if (dst_cap < src_len * 2 + 1) return AGENT_ERR_BAD_ARG;
    for (size_t i = 0; i < src_len; i++) {
        dst[i * 2]     = kHex[(src[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = kHex[src[i] & 0x0F];
    }
    dst[src_len * 2] = '\0';
    return AGENT_OK;
}

static int from_hex(const char *src, size_t src_len, uint8_t *dst, size_t dst_cap)
{
    if (src_len % 2 != 0) return AGENT_ERR_BAD_ARG;
    if (dst_cap < src_len / 2) return AGENT_ERR_BAD_ARG;
    for (size_t i = 0; i < src_len / 2; i++) {
        char hi = src[i * 2], lo = src[i * 2 + 1];
        uint8_t h = (hi >= '0' && hi <= '9') ? (hi - '0') :
                    (hi >= 'a' && hi <= 'f') ? (hi - 'a' + 10) :
                    (hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : 0xFF;
        uint8_t l = (lo >= '0' && lo <= '9') ? (lo - '0') :
                    (lo >= 'a' && lo <= 'f') ? (lo - 'a' + 10) :
                    (lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : 0xFF;
        if (h == 0xFF || l == 0xFF) return AGENT_ERR_BAD_ARG;
        dst[i] = (h << 4) | l;
    }
    return AGENT_OK;
}

int llm_dpapi_encrypt_hex(const char *plaintext, size_t plaintext_len,
                          char *hex_out, size_t hex_out_cap)
{
    if (!plaintext || !hex_out) return AGENT_ERR_BAD_ARG;
    DATA_BLOB in = { plaintext_len, (BYTE *)plaintext };
    DATA_BLOB out = { 0, NULL };
    if (!CryptProtectData(&in, L"modem-agent-llm-key", NULL, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return AGENT_ERR_IO;
    }
    int rc = to_hex(out.pbData, out.cbData, hex_out, hex_out_cap);
    LocalFree(out.pbData);
    return rc;
}

int llm_dpapi_decrypt_hex(const char *hex_in, size_t hex_len,
                          char *plaintext_out, size_t plaintext_out_cap)
{
    if (!hex_in || !plaintext_out) return AGENT_ERR_BAD_ARG;
    uint8_t enc[4096];
    int rc = from_hex(hex_in, hex_len, enc, sizeof(enc));
    if (rc != AGENT_OK) return rc;
    DATA_BLOB in = { hex_len / 2, enc };
    DATA_BLOB out = { 0, NULL };
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return AGENT_ERR_IO;
    }
    if (out.cbData >= plaintext_out_cap) {
        LocalFree(out.pbData);
        return AGENT_ERR_BAD_ARG;
    }
    memcpy(plaintext_out, out.pbData, out.cbData);
    plaintext_out[out.cbData] = '\0';
    LocalFree(out.pbData);
    return AGENT_OK;
}
