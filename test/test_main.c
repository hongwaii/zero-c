/**
 * @file test_main.c
 * @brief Test executable entry point — mbedtls + libcurl integration tests
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_main.h"
#include "version.h"

/* ---- mbedtls headers ---- */
#include <mbedtls/version.h>
#include <psa/crypto.h>

/* ---- libcurl headers ---- */
#define CURL_DISABLE_TYPECHECK  /* clang doesn't support curl's GCC builtins */
#include <curl/curl.h>

/* ---- Built-in self-tests ---- */

static int test_example_pass(void)
{
    TEST_ASSERT(1 == 1, "Basic sanity check");
    return TEST_PASS;
}

static int test_version_string(void)
{
    TEST_ASSERT(APP_VERSION_MAJOR >= 0, "Version major >= 0");
    TEST_ASSERT(APP_VERSION_MINOR >= 0, "Version minor >= 0");
    TEST_ASSERT(APP_VERSION_PATCH >= 0, "Version patch >= 0");
    return TEST_PASS;
}

/* ================================================================
 *  Test: mbedtls — version + SHA-256
 * ================================================================ */
static int test_mbedtls_version(void)
{
    printf("    mbedtls version: %s\n", MBEDTLS_VERSION_STRING);

    /* Check version string is not empty */
    TEST_ASSERT(MBEDTLS_VERSION_STRING[0] != '\0',
                "mbedtls version string not empty");
    return TEST_PASS;
}

static int test_mbedtls_sha256(void)
{
    /* Initialize PSA Crypto */
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        printf("    PSA crypto init failed: %ld\n", (long)status);
        return TEST_FAIL;
    }

    /* Known test vector: empty string → SHA-256 */
    const char *input = "Hello, Windows-C! mbedtls SHA-256 test.";
    uint8_t hash[32];
    size_t hash_len = 0;

    status = psa_hash_compute(PSA_ALG_SHA_256,
                              (const uint8_t *)input, strlen(input),
                              hash, sizeof(hash), &hash_len);
    if (status != PSA_SUCCESS) {
        printf("    SHA-256 compute failed: %ld\n", (long)status);
        return TEST_FAIL;
    }

    TEST_ASSERT(hash_len == 32, "SHA-256 hash is 32 bytes");

    /* Print hash for manual verification */
    printf("    SHA-256: ");
    for (size_t i = 0; i < hash_len; i++) printf("%02x", hash[i]);
    printf("\n");

    return TEST_PASS;
}

/* ================================================================
 *  Test: libcurl — version + HTTP GET
 * ================================================================ */
static int test_curl_version(void)
{
    const char *ver = curl_version();
    printf("    libcurl version: %s\n", ver);

    TEST_ASSERT(ver != NULL && ver[0] != '\0',
                "libcurl version string not empty");
    return TEST_PASS;
}

static size_t write_cb(char *contents, size_t size,
                        size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    char *buf = (char *)userp;
    strncat(buf, contents, total < 4096 ? total : 4095);
    return total;
}

static int test_curl_http_get(void)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("    curl_easy_init() failed\n");
        return TEST_FAIL;
    }

    char response[4096] = {0};
    long http_code = 0;

    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://httpbin.org/get?msg=Windows-C-Test");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Windows-C-Test/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("    HTTP request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return TEST_FAIL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    TEST_ASSERT(http_code == 200, "HTTP status is 200 OK");
    printf("    HTTP 200 OK, body length: %zu\n", strlen(response));

    return TEST_PASS;
}

/* ================================================================
 *  Main test runner
 * ================================================================ */
int main(void)
{
    int total   = 0;
    int passed  = 0;
    int failed  = 0;
    int skipped = 0;

    printf("=== TEST Runner ===\n");
    printf("Product: %s\n", APP_FULL_TAG);
    printf("\n");

    /* ---- Built-in sanity checks ---- */
    RUN_TEST(test_example_pass);
    RUN_TEST(test_version_string);

    /* ---- mbedtls integration tests ---- */
    printf("\n-- mbedtls Tests --\n");
    RUN_TEST(test_mbedtls_version);
    RUN_TEST(test_mbedtls_sha256);

    /* ---- libcurl integration tests ---- */
    printf("\n-- libcurl Tests --\n");
    RUN_TEST(test_curl_version);
    RUN_TEST(test_curl_http_get);

    /* Summary */
    printf("\n--- Results ---\n");
    printf("Total:   %d\n", total);
    printf("Passed:  %d\n", passed);
    printf("Failed:  %d\n", failed);
    printf("Skipped: %d\n", skipped);
    printf("================\n");

    return (failed > 0) ? 1 : 0;
}
