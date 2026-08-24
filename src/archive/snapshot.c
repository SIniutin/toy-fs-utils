#define _XOPEN_SOURCE 700

#include "archive/paths.h"
#include "archive/snapshot.h"
#include "utils/common.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int archive_file_is_unchanged(const struct stat *orig, const struct stat *bak)
{
    if (!orig || !bak)
        return 0;
    if (orig->st_ino == bak->st_ino && orig->st_dev == bak->st_dev)
        return 1;
    if (orig->st_size == bak->st_size && orig->st_mtime == bak->st_mtime)
        return 1;
    return 0;
}

archive_file_status_t archive_compare_with_dst(const struct stat *orig, const char *dst, const char *tool)
{
    struct stat st_bak;
    if (stat(dst, &st_bak) != 0)
    {
        if (errno == ENOENT)
            return ARCHIVE_FILE_NEW;
        fprintf(stderr, "%s: stat(%s) failed: %s\n", tool, dst, strerror(errno));
        return ARCHIVE_FILE_CHANGED;
    }
    return archive_file_is_unchanged(orig, &st_bak) ? ARCHIVE_FILE_UNCHANGED : ARCHIVE_FILE_CHANGED;
}

int archive_version_path(const char *backup_root, const char *rel, const char *datetime, char *out, size_t outsz)
{
    char versions_dir[PATH_MAX];
    if (archive_versions_dir(versions_dir, sizeof(versions_dir), backup_root) != 0)
        return -1;

    char base_ver_path[PATH_MAX];
    if (snprintf_checked(base_ver_path, sizeof(base_ver_path), "%s/%s@%s", versions_dir, rel, datetime) != 0)
        return -1;

    struct stat tmp;
    int idx = 0;
    for (;;)
    {
        if (idx == 0)
        {
            if (snprintf_checked(out, outsz, "%s", base_ver_path) != 0)
                return -1;
        }
        else
        {
            if (snprintf_checked(out, outsz, "%s.%d", base_ver_path, idx) != 0)
                return -1;
        }

        if (stat(out, &tmp) != 0)
            return 0;
        idx++;
    }
}
