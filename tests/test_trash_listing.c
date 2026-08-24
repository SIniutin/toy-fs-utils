#define _XOPEN_SOURCE 700

#include "trashbin/listing.h"

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

static trash_log_entry_t entry(const char *original, long size, const char *timestamp)
{
    trash_log_entry_t e;
    memset(&e, 0, sizeof(e));
    snprintf(e.original, sizeof(e.original), "%s", original);
    snprintf(e.link, sizeof(e.link), "%s.link", original);
    e.size = size;
    snprintf(e.timestamp, sizeof(e.timestamp), "%s", timestamp);
    return e;
}

static int test_parse_limit(void)
{
    long limit = -1;

    ASSERT_TRUE(trash_listing_parse_limit("0", &limit) == 0);
    ASSERT_TRUE(limit == 0);
    ASSERT_TRUE(trash_listing_parse_limit("42", &limit) == 0);
    ASSERT_TRUE(limit == 42);

    ASSERT_TRUE(trash_listing_parse_limit(NULL, &limit) != 0);
    ASSERT_TRUE(trash_listing_parse_limit("abc", &limit) != 0);
    ASSERT_TRUE(trash_listing_parse_limit("-1", &limit) != 0);
    ASSERT_TRUE(trash_listing_parse_limit("12x", &limit) != 0);
    ASSERT_TRUE(trash_listing_parse_limit("12", NULL) != 0);

    return 0;
}

static int test_filter_basename(void)
{
    trash_log_entry_t entries[] = {
        entry("/tmp/alpha.txt", 10, "2026-01-01"),
        entry("/tmp/beta.log", 20, "2026-01-02"),
        entry("/tmp/nested/alpha.log", 30, "2026-01-03"),
    };

    size_t len = trash_listing_filter_basename(entries, 3, "alpha");
    ASSERT_TRUE(len == 2);
    ASSERT_TRUE(strcmp(entries[0].original, "/tmp/alpha.txt") == 0);
    ASSERT_TRUE(strcmp(entries[1].original, "/tmp/nested/alpha.log") == 0);

    len = trash_listing_filter_basename(entries, len, "");
    ASSERT_TRUE(len == 2);

    len = trash_listing_filter_basename(NULL, 7, "alpha");
    ASSERT_TRUE(len == 7);

    return 0;
}

static int test_sort_and_limit(void)
{
    trash_log_entry_t entries[] = {
        entry("/tmp/small", 5, "2026-01-01"),
        entry("/tmp/large", 50, "2026-01-03"),
        entry("/tmp/mid", 20, "2026-01-02"),
    };

    trash_listing_sort(entries, 3, TRASH_LIST_SORT_SIZE);
    ASSERT_TRUE(strcmp(entries[0].original, "/tmp/large") == 0);
    ASSERT_TRUE(strcmp(entries[1].original, "/tmp/mid") == 0);
    ASSERT_TRUE(strcmp(entries[2].original, "/tmp/small") == 0);

    trash_listing_sort(entries, 3, TRASH_LIST_SORT_TIME);
    ASSERT_TRUE(strcmp(entries[0].original, "/tmp/large") == 0);
    ASSERT_TRUE(strcmp(entries[1].original, "/tmp/mid") == 0);
    ASSERT_TRUE(strcmp(entries[2].original, "/tmp/small") == 0);

    trash_listing_sort(entries, 0, TRASH_LIST_SORT_SIZE);
    trash_listing_sort(NULL, 3, TRASH_LIST_SORT_TIME);
    trash_listing_sort(entries, 3, TRASH_LIST_SORT_NONE);

    ASSERT_TRUE(trash_listing_apply_limit(3, -1) == 3);
    ASSERT_TRUE(trash_listing_apply_limit(3, 4) == 3);
    ASSERT_TRUE(trash_listing_apply_limit(3, 2) == 2);
    ASSERT_TRUE(trash_listing_apply_limit(3, 0) == 0);

    return 0;
}

int main(void)
{
    if (test_parse_limit() != 0)
        return 1;
    if (test_filter_basename() != 0)
        return 1;
    if (test_sort_and_limit() != 0)
        return 1;
    return 0;
}
