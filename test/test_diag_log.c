/**
 * @file test_diag_log.c
 * @brief P4 diag_log 单元测试。
 *
 * 范围：只测 miniz 压 zip 流程（不需要真串口 / 真 at_session）。
 *
 * 不调 diag_capture_log（依赖 at_session mock 太重），直接：
 *   1) 写一个临时 .log 文件
 *   2) 用 miniz API 把它压成 zip
 *   3) 验证 zip 文件存在 + 非空
 *   4) 验证 zip 内含 "modem.log" 条目
 *
 * 输出文件全部放在 ./out/ 子目录（运行时 cwd 已经在 build 目录）。
 */
#include "agent_types.h"

/* miniz 在 third_party/miniz/，通过 lib_diag_service 转出 PUBLIC include */
#include "miniz.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

/* 测试产物路径：相对 cwd（build 目录）。 */
#define LOG_IN_PATH    "out/test_diag_log_input.log"
#define ZIP_OUT_PATH   "out/test_diag_log_out.zip"

/* 写测试输入 log：模拟模组 dump 出来的文本（多行 AT 风格） */
static void write_sample_log(const char *path)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    const char *content =
        "2026-06-07 12:00:00 [AT] AT+CAPTURELOG=start\n"
        "2026-06-07 12:00:01 [RX] +CAPTURELOG: 0\n"
        "2026-06-07 12:00:02 [TX] hello modem log capture\n"
        "2026-06-07 12:00:03 [RX] OK\n"
        "2026-06-07 12:00:30 [AT] AT+CAPTURELOG=stop\n"
        "2026-06-07 12:00:31 [RX] +CAPTURELOG: 1024 bytes\n"
        "2026-06-07 12:00:32 [RX] OK\n";
    size_t n = strlen(content);
    size_t wrote = fwrite(content, 1, n, f);
    assert(wrote == n);
    fclose(f);
    (void)wrote;  /* release 模式 assert 被宏掉，wrote 标记为 used */
}

/* 取文件大小（stat）；不存在返回 0。 */
static long file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (long)st.st_size;
}

int main(void)
{
    /* 确保 out/ 目录存在（build 时 cmake 已建，保险起见再确认） */
    mkdir("out");           /* MinGW: 单参数版；已存在会返回 -1，忽略 */

    /* 1) 写 tmp log */
    write_sample_log(LOG_IN_PATH);
    long in_sz = file_size(LOG_IN_PATH);
    assert(in_sz > 0);
    printf("test_diag_log: input log = %ld bytes\n", in_sz);

    /* 2) miniz 压 zip（这正是 diag_log.c 内部 log_finish 用的同款调用） */
    mz_zip_archive zip = {0};
    int rc = mz_zip_writer_init_file(&zip, ZIP_OUT_PATH, 0);
    assert(rc && "mz_zip_writer_init_file failed");

    rc = mz_zip_writer_add_file(&zip, "modem.log", LOG_IN_PATH,
                                NULL, 0, MZ_DEFAULT_LEVEL);
    assert(rc && "mz_zip_writer_add_file failed");

    rc = mz_zip_writer_finalize_archive(&zip);
    assert(rc && "mz_zip_writer_finalize_archive failed");
    mz_zip_writer_end(&zip);

    /* 3) 验证 zip 文件存在 + 非空（N > 0） */
    long zip_sz = file_size(ZIP_OUT_PATH);
    assert(zip_sz > 0 && "zip file is empty");
    printf("test_diag_log: zip       = %ld bytes\n", zip_sz);

    /* 4) 验证 zip 内含 "modem.log" 条目（打开 reader 数 entries） */
    mz_zip_archive rzip = {0};
    assert(mz_zip_reader_init_file(&rzip, ZIP_OUT_PATH, 0));
    mz_uint num = mz_zip_reader_get_num_files(&rzip);
    assert(num == 1 && "expected exactly 1 entry in zip");
    /* 验证文件名是 modem.log */
    mz_uint sz = mz_zip_reader_get_filename(&rzip, 0, NULL, 0);
    assert(sz > 0);
    char *name = (char *)malloc(sz);
    assert(name);
    mz_zip_reader_get_filename(&rzip, 0, name, sz);
    assert(strcmp(name, "modem.log") == 0);
    free(name);
    mz_zip_reader_end(&rzip);

    /* 清理 */
    remove(LOG_IN_PATH);
    remove(ZIP_OUT_PATH);

    printf("test_diag_log: all pass (zip=%ld bytes, 1 entry 'modem.log')\n", zip_sz);
    return 0;
}
