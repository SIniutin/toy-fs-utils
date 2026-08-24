#define _XOPEN_SOURCE 700

#include "utils/common.h"

#include <ctype.h>
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

typedef struct
{
    char original[PATH_MAX];
    char link[PATH_MAX];
    unsigned long inode;
    long size;
    char timestamp[64];
} entry_t;

static int parse_limit(const char *s, long *out)
{
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 0)
        return -1;
    *out = v;
    return 0;
}

static char *trim(char *s)
{
    while (*s == ' ')
        s++;

    size_t n = strlen(s);
    if (n == 0)
        return s;

    char *end = s + n - 1;
    while (end > s && *end == ' ')
        *end-- = '\0';

    return s;
}

int cmp_size(const void *a, const void *b)
{
    const entry_t *x = a, *y = b;
    if (y->size < x->size)
        return -1;
    if (y->size > x->size)
        return 1;
    return 0;
}

int cmp_time(const void *a, const void *b)
{
    const entry_t *x = a, *y = b;
    return strcmp(y->timestamp, x->timestamp);
}

int main(int argc, char *argv[])
{
    char grep_pat[256] = "";
    int sort_mode = 0;
    long limit = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0)
            sort_mode = 1;
        else if (strcmp(argv[i], "-t") == 0)
            sort_mode = 2;
        else if (strcmp(argv[i], "-n") == 0)
        {
            if (i + 1 >= argc || parse_limit(argv[i + 1], &limit) != 0)
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
    snprintf(log_path, sizeof(log_path), "%s/.trash.log", home);

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

    entry_t *arr = NULL;
    size_t cap = 0, len = 0;

    char line[4096];

    while (fgets(line, sizeof(line), f))
    {
        entry_t tmp;

        char *p = strtok(line, "|");
        if (!p)
            continue;
        snprintf(tmp.original, sizeof(tmp.original), "%s", trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        snprintf(tmp.link, sizeof(tmp.link), "%s", trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        tmp.inode = strtoul(trim(p), NULL, 10);

        p = strtok(NULL, "|");
        if (!p)
            continue;
        tmp.size = atol(trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        snprintf(tmp.timestamp, sizeof(tmp.timestamp), "%s", trim(p));

        if (len == cap)
        {
            cap = cap ? cap * 2 : 64;
            entry_t *tmp_arr = realloc(arr, cap * sizeof(*arr));
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

    if (grep_pat[0])
    {
        size_t j = 0;
        for (size_t i = 0; i < len; i++)
        {
            if (strstr(get_basename(arr[i].original), grep_pat))
                arr[j++] = arr[i];
        }
        len = j;
    }

    if (sort_mode == 1)
        qsort(arr, len, sizeof(entry_t), cmp_size);
    else if (sort_mode == 2)
        qsort(arr, len, sizeof(entry_t), cmp_time);

    if (limit >= 0 && (size_t)limit < len)
        len = limit;

    printf("%-40s %-40s %-10s %-10s %-14s\n", "ORIGINAL_PATH", "LINK_NAME", "INODE", "SIZE", "TIMESTAMP");

    for (size_t i = 0; i < len; i++)
    {
        printf("%-40s %-40s %-10lu %-10ld %-14s\n", arr[i].original, arr[i].link, arr[i].inode, arr[i].size, arr[i].timestamp);
    }

    free(arr);
    return 0;
}
