#define _XOPEN_SOURCE 700

#include "archive/paths.h"
#include "utils/common.h"

#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    char name[PATH_MAX];
    long files;
    long versions;
    long long total_sz;
    char date_key[9];
} backup_info_t;

static int ensure_is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

static int is_regular_file(const struct stat *st)
{
    return S_ISREG(st->st_mode);
}

static int scan_total(const char *path, long long *total)
{
    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char child[PATH_MAX];
        if (join_path_into(child, sizeof(child), path, ent->d_name) != 0)
            continue;

        struct stat st;
        if (lstat(child, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            scan_total(child, total);
        }
        else if (is_regular_file(&st))
        {
            *total += st.st_size;
        }
    }

    closedir(d);
    return 0;
}

static int scan_files_without_versions(const char *path, long *count)
{
    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        if (strcmp(ent->d_name, ".versions") == 0)
            continue;

        char child[PATH_MAX];
        if (join_path_into(child, sizeof(child), path, ent->d_name) != 0)
            continue;

        struct stat st;
        if (lstat(child, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            scan_files_without_versions(child, count);
        }
        else if (is_regular_file(&st))
        {
            (*count)++;
        }
    }

    closedir(d);
    return 0;
}

static int scan_versions(const char *path, long *count)
{
    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char child[PATH_MAX];
        if (join_path_into(child, sizeof(child), path, ent->d_name) != 0)
            continue;

        struct stat st;
        if (lstat(child, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            scan_versions(child, count);
        }
        else if (is_regular_file(&st))
        {
            (*count)++;
        }
    }

    closedir(d);
    return 0;
}

static void parse_date_key(const char *name, char out[9])
{
    size_t len = strlen(name);
    if (len >= 10)
    {
        const char *p = name + len - 10;
        if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) &&
            isdigit((unsigned char)p[3]) && p[4] == '-' && isdigit((unsigned char)p[5]) &&
            isdigit((unsigned char)p[6]) && p[7] == '-' && isdigit((unsigned char)p[8]) && isdigit((unsigned char)p[9]))
        {
            out[0] = p[0];
            out[1] = p[1];
            out[2] = p[2];
            out[3] = p[3];
            out[4] = p[5];
            out[5] = p[6];
            out[6] = p[8];
            out[7] = p[9];
            out[8] = '\0';
            return;
        }
    }
    strcpy(out, "00000000");
}

static int cmp_by_date_desc(const void *a, const void *b)
{
    const backup_info_t *x = a;
    const backup_info_t *y = b;
    int c = strcmp(y->date_key, x->date_key);
    if (c != 0)
        return c;
    return strcmp(x->name, y->name);
}

static int cmp_by_size_desc(const void *a, const void *b)
{
    const backup_info_t *x = a;
    const backup_info_t *y = b;
    if (y->total_sz > x->total_sz)
        return 1;
    if (y->total_sz < x->total_sz)
        return -1;
    return strcmp(x->name, y->name);
}

int main(int argc, char *argv[])
{
    int sort_mode = 1;
    long limit = -1;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            sort_mode = 1;
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            sort_mode = 2;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "backup_list: -n requires integer\n");
                return 1;
            }
            limit = atol(argv[++i]);
            if (limit < 0)
                limit = -1;
        }
        else
        {
            fprintf(stderr, "Usage: backup_list [-n N] [-s|-t]\n");
            return 1;
        }
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "backup_list: HOME is not set\n");
        return 1;
    }

    char backups_dir[PATH_MAX];
    if (archive_backups_dir(backups_dir, sizeof(backups_dir)) != 0)
    {
        fprintf(stderr, "backup_list: Backups path is too long\n");
        return 1;
    }

    if (!ensure_is_dir(backups_dir))
    {
        fprintf(stderr, "backup_list: no backups directory: %s\n", backups_dir);
        return 0;
    }

    DIR *d = opendir(backups_dir);
    if (!d)
    {
        perror("backup_list: opendir");
        return 1;
    }

    backup_info_t *arr = NULL;
    size_t len = 0, cap = 0;

    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char bpath[PATH_MAX];
        if (join_path_into(bpath, sizeof(bpath), backups_dir, ent->d_name) != 0)
        {
            fprintf(stderr, "backup_list: backup path is too long: %s/%s\n", backups_dir, ent->d_name);
            continue;
        }

        struct stat st;
        if (stat(bpath, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        if (len == cap)
        {
            size_t ncap = cap ? cap * 2 : 16;
            backup_info_t *tmp = realloc(arr, ncap * sizeof(backup_info_t));
            if (!tmp)
            {
                perror("backup_list: realloc");
                closedir(d);
                free(arr);
                return 1;
            }
            arr = tmp;
            cap = ncap;
        }

        backup_info_t *info = &arr[len];
        memset(info, 0, sizeof(*info));
        snprintf(info->name, sizeof(info->name), "%s", ent->d_name);
        parse_date_key(info->name, info->date_key);

        long files = 0;
        scan_files_without_versions(bpath, &files);
        info->files = files;

        char vdir[PATH_MAX];
        if (archive_versions_dir(vdir, sizeof(vdir), bpath) != 0)
        {
            fprintf(stderr, "backup_list: versions path is too long: %s/.versions\n", bpath);
            continue;
        }
        long versions = 0;
        if (ensure_is_dir(vdir))
        {
            scan_versions(vdir, &versions);
        }
        info->versions = versions;

        long long total = 0;
        scan_total(bpath, &total);
        info->total_sz = total;

        len++;
    }

    closedir(d);

    if (len == 0)
    {
        fprintf(stderr, "backup_list: no backups found in %s\n", backups_dir);
        free(arr);
        return 0;
    }

    if (sort_mode == 2)
        qsort(arr, len, sizeof(backup_info_t), cmp_by_size_desc);
    else
        qsort(arr, len, sizeof(backup_info_t), cmp_by_date_desc);

    if (limit >= 0 && (size_t)limit < len)
        len = (size_t)limit;

    printf("Available backups:\n");
    for (size_t i = 0; i < len; ++i)
    {
        long long kb = (arr[i].total_sz + 1023) / 1024;
        printf("%s | %ld files, %ld versions, %lldK total\n", arr[i].name, arr[i].files, arr[i].versions, kb);
    }

    free(arr);
    return 0;
}
