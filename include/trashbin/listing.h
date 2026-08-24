#ifndef TRASHBIN_LISTING_H
#define TRASHBIN_LISTING_H

#include "trashbin/log.h"

#include <stddef.h>

typedef enum
{
    TRASH_LIST_SORT_NONE = 0,
    TRASH_LIST_SORT_SIZE = 1,
    TRASH_LIST_SORT_TIME = 2
} trash_list_sort_t;

int trash_listing_parse_limit(const char *s, long *out);
size_t trash_listing_filter_basename(trash_log_entry_t *entries, size_t len, const char *pattern);
void trash_listing_sort(trash_log_entry_t *entries, size_t len, trash_list_sort_t sort_mode);
size_t trash_listing_apply_limit(size_t len, long limit);

#endif
