/**
 * @file test_main.h
 * @brief Test framework header
 */
#ifndef TEST_MAIN_H
#define TEST_MAIN_H

/* Test result codes */
#define TEST_PASS  0
#define TEST_FAIL  1
#define TEST_SKIP  2

/* Simple assertion helpers */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        return TEST_FAIL; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s — expected %d, got %d (%s:%d)\n", msg, (int)(b), (int)(a), __FILE__, __LINE__); \
        return TEST_FAIL; \
    } \
} while(0)

/* Test runner macro */
#define RUN_TEST(func) do { \
    printf("  Running %s... ", #func); \
    int _r = func(); \
    if (_r == TEST_PASS) { \
        printf("PASS\n"); \
        passed++; \
    } else if (_r == TEST_FAIL) { \
        failed++; \
    } else { \
        printf("SKIP\n"); \
        skipped++; \
    } \
    total++; \
} while(0)

#endif /* TEST_MAIN_H */
