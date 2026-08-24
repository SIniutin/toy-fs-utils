#define _XOPEN_SOURCE 700

#include "trashbin/listing.h"
#include "utils/common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int trash_listing_parse_limit(const char *s, long *out)
{
    if (!s || !out)
        return -1;

    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 0)
        return -1;

    *out = v;
    return 0;
}

size_t trash_listing_filter_basename(trash_log_entry_t *entries, size_t len, const char *pattern)
{
    if (!entries || !pattern || pattern[0] == '\0')
        return len;

    size_t out = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (strstr(get_basename(entries[i].original), pattern))
            entries[out++] = entries[i];
    }
    return out;
}

static int cmp_size_desc(const void *a, const void *b)
{
    const trash_log_entry_t *x = a, *y = b;
    if (y->size < x->size)
        return -1;
    if (y->size > x->size)
        return 1;
    return 0;
}

static int cmp_time_desc(const void *a, const void *b)
{
    const trash_log_entry_t *x = a, *y = b;
    return strcmp(y->timestamp, x->timestamp);
}

void trash_listing_sort(trash_log_entry_t *entries, size_t len, trash_list_sort_t sort_mode)
{
    if (!entries || len == 0)
        return;

    if (sort_mode == TRASH_LIST_SORT_SIZE)
        qsort(entries, len, sizeof(*entries), cmp_size_desc);
    else if (sort_mode == TRASH_LIST_SORT_TIME)
        qsort(entries, len, sizeof(*entries), cmp_time_desc);
}

size_t trash_listing_apply_limit(size_t len, long limit)
{
    if (limit >= 0 && (size_t)limit < len)
        return (size_t)limit;
    return len;
}
