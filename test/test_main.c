/**
 * @file test_main.c
 * @brief Test executable entry point
 */
#include <stdio.h>
#include "test_main.h"
#include "version.h"

/* ---- Example test cases ---- */

static int test_example_pass(void)
{
    TEST_ASSERT(1 == 1, "Basic sanity check");
    return TEST_PASS;
}

static int test_example_skip(void)
{
    /* Feature not yet implemented */
    return TEST_SKIP;
}

static int test_version_string(void)
{
    TEST_ASSERT(APP_VERSION_MAJOR >= 0, "Version major >= 0");
    TEST_ASSERT(APP_VERSION_MINOR >= 0, "Version minor >= 0");
    TEST_ASSERT(APP_VERSION_PATCH >= 0, "Version patch >= 0");
    return TEST_PASS;
}

/* ---- Main test runner ---- */

int main(void)
{
    int total   = 0;
    int passed  = 0;
    int failed  = 0;
    int skipped = 0;

    printf("=== TEST Runner ===\n");
    printf("Product: %s\n", APP_FULL_TAG);
    printf("\n");

    /* Register and run all tests */
    RUN_TEST(test_example_pass);
    RUN_TEST(test_example_skip);
    RUN_TEST(test_version_string);

    /* TODO: Add your test cases here:
       RUN_TEST(test_http_init);
    */

    /* Summary */
    printf("\n--- Results ---\n");
    printf("Total:   %d\n", total);
    printf("Passed:  %d\n", passed);
    printf("Failed:  %d\n", failed);
    printf("Skipped: %d\n", skipped);
    printf("================\n");

    return (failed > 0) ? 1 : 0;
}
