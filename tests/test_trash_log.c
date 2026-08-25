#define _XOPEN_SOURCE 700

#include "trashbin/log.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                                              \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(expr))                                                                                                  \
        {                                                                                                             \
            fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                              \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

static int test_parse_valid_line(void)
{
    char line[] = "/tmp/file.txt | trash-123 | 42 | 8192 | 2026-08-24_15-01-02\n";
    trash_log_entry_t e;

    ASSERT_TRUE(trash_log_parse_line(line, &e) == 0);
    ASSERT_TRUE(strcmp(e.original, "/tmp/file.txt") == 0);
    ASSERT_TRUE(strcmp(e.link, "trash-123") == 0);
    ASSERT_TRUE(e.inode == 42);
    ASSERT_TRUE(e.size == 8192);
    ASSERT_TRUE(strcmp(e.timestamp, "2026-08-24_15-01-02") == 0);

    return 0;
}

static int test_parse_trims_tabs_and_carriage_return(void)
{
    char line[] = "\t/a/b\t|\tlink\t|\t7\t|\t9\t|\t20260824150102\r\n";
    trash_log_entry_t e;

    ASSERT_TRUE(trash_log_parse_line(line, &e) == 0);
    ASSERT_TRUE(strcmp(e.original, "/a/b") == 0);
    ASSERT_TRUE(strcmp(e.link, "link") == 0);
    ASSERT_TRUE(e.inode == 7);
    ASSERT_TRUE(e.size == 9);
    ASSERT_TRUE(strcmp(e.timestamp, "20260824150102") == 0);

    return 0;
}

static int test_parse_rejects_missing_fields(void)
{
    char no_link[] = "/tmp/file|";
    char no_timestamp[] = "/tmp/file|link|1|2";
    trash_log_entry_t e;

    ASSERT_TRUE(trash_log_parse_line(NULL, &e) != 0);
    ASSERT_TRUE(trash_log_parse_line(no_link, &e) != 0);
    ASSERT_TRUE(trash_log_parse_line(no_timestamp, &e) != 0);
    ASSERT_TRUE(trash_log_parse_line(no_timestamp, NULL) != 0);

    return 0;
}

static int test_parse_keeps_legacy_number_behavior(void)
{
    char line[] = "/tmp/file|link|abc|size|ts";
    trash_log_entry_t e;

    ASSERT_TRUE(trash_log_parse_line(line, &e) == 0);
    ASSERT_TRUE(e.inode == 0);
    ASSERT_TRUE(e.size == 0);
    ASSERT_TRUE(strcmp(e.timestamp, "ts") == 0);

    return 0;
}

int main(void)
{
    if (test_parse_valid_line() != 0)
        return 1;
    if (test_parse_trims_tabs_and_carriage_return() != 0)
        return 1;
    if (test_parse_rejects_missing_fields() != 0)
        return 1;
    if (test_parse_keeps_legacy_number_behavior() != 0)
        return 1;
    return 0;
}
