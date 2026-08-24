#define _XOPEN_SOURCE 700

#include "trashbin/listing.h"
#include "trashbin/log.h"
#include "trashbin/paths.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_OK 0
#define LIST_USAGE 1
#define LIST_IO 2
#define LIST_INTERNAL 3

int main(int argc, char *argv[])
{
    char grep_pat[256] = "";
    trash_list_sort_t sort_mode = TRASH_LIST_SORT_NONE;
    long limit = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0)
            sort_mode = TRASH_LIST_SORT_SIZE;
        else if (strcmp(argv[i], "-t") == 0)
            sort_mode = TRASH_LIST_SORT_TIME;
        else if (strcmp(argv[i], "-n") == 0)
        {
            if (i + 1 >= argc || trash_listing_parse_limit(argv[i + 1], &limit) != 0)
            {
                fprintf(stderr, "list_trash: -n requires a non-negative integer\n");
                return LIST_USAGE;
            }
            i++;
        }
        else if (strcmp(argv[i], "--grep") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "list_trash: --grep requires a pattern\n");
                return LIST_USAGE;
            }
            snprintf(grep_pat, sizeof(grep_pat), "%s", argv[++i]);
        }
        else
        {
            fprintf(stderr, "list_trash: unknown option: %s\n", argv[i]);
            return LIST_USAGE;
        }
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "list_trash: HOME not set\n");
        return LIST_USAGE;
    }

    char log_path[PATH_MAX];
    if (trash_log_path(log_path, sizeof(log_path)) != 0)
    {
        fprintf(stderr, "list_trash: log path is too long\n");
        return LIST_IO;
    }

    FILE *f = fopen(log_path, "r");
    if (!f)
    {
        if (errno == ENOENT)
        {
            fprintf(stderr, "list_trash: trash is empty\n");
            return LIST_OK;
        }
        perror("list_trash: open ~/.trash.log");
        return LIST_IO;
    }

    trash_log_entry_t *arr = NULL;
    size_t cap = 0, len = 0;

    char line[4096];

    while (fgets(line, sizeof(line), f))
    {
        trash_log_entry_t tmp;
        if (trash_log_parse_line(line, &tmp) != 0)
            continue;

        if (len == cap)
        {
            cap = cap ? cap * 2 : 64;
            trash_log_entry_t *tmp_arr = realloc(arr, cap * sizeof(*arr));
            if (!tmp_arr)
            {
                fclose(f);
                free(arr);
                perror("list_trash: realloc");
                return LIST_INTERNAL;
            }
            arr = tmp_arr;
        }

        arr[len++] = tmp;
    }

    if (ferror(f))
    {
        perror("list_trash: read ~/.trash.log");
        fclose(f);
        free(arr);
        return LIST_IO;
    }

    fclose(f);

    len = trash_listing_filter_basename(arr, len, grep_pat);
    trash_listing_sort(arr, len, sort_mode);
    len = trash_listing_apply_limit(len, limit);

    printf("%-40s %-40s %-10s %-10s %-14s\n", "ORIGINAL_PATH", "LINK_NAME", "INODE", "SIZE", "TIMESTAMP");

    for (size_t i = 0; i < len; i++)
    {
        printf("%-40s %-40s %-10lu %-10ld %-14s\n", arr[i].original, arr[i].link, arr[i].inode, arr[i].size, arr[i].timestamp);
    }

    free(arr);
    return 0;
}
