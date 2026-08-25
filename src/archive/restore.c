#define _XOPEN_SOURCE 700

#include "archive/file_ops.h"
#include "archive/restore.h"
#include "utils/common.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int path_is_dir(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int resolve_dir_into(const char *path, char *out, size_t outsz)
{
    char resolved[PATH_MAX];
    if (!realpath(path, resolved) || !path_is_dir(resolved))
        return -1;
    return snprintf_checked(out, outsz, "%s", resolved);
}

int archive_restore_should_skip_rel(const char *rel)
{
    if (!rel || rel[0] == '\0')
        return 1;
    if (strncmp(rel, ".versions", 9) == 0 && (rel[9] == '/' || rel[9] == '\0'))
        return 1;
    if (strcmp(rel, "versions.tar.gz") == 0)
        return 1;
    if (strcmp(rel, ".source_path") == 0)
        return 1;
    return 0;
}

int archive_restore_resolve_dest_root(const char *backup_root, const char *to_dir, char *out, size_t outsz)
{
    if (!backup_root || !out)
        return -1;

    if (to_dir && to_dir[0])
    {
        if (realpath(to_dir, out))
            return path_is_dir(out) ? 0 : -1;
        if (snprintf_checked(out, outsz, "%s", to_dir) != 0)
            return -1;
        if (mkdir_p(out, 0755) != 0)
            return -1;
        return resolve_dir_into(out, out, outsz);
    }

    char src_restore[PATH_MAX];
    if (archive_read_source_path(backup_root, src_restore, sizeof(src_restore)) != 0)
        return -1;

    if (!realpath(src_restore, out))
    {
        if (mkdir_p(src_restore, 0755) != 0 || !realpath(src_restore, out))
            return -1;
    }
    if (!path_is_dir(out))
        return -1;
    if (mkdir_p(out, 0755) != 0)
        return -1;
    return resolve_dir_into(out, out, outsz);
}

int archive_restore_dest_path(const char *dest_root, const char *rel, char *out, size_t outsz)
{
    if (!dest_root || !rel || !out || archive_restore_should_skip_rel(rel))
        return -1;
    return join_path_into(out, outsz, dest_root, rel);
}

int archive_restore_ensure_parent_dir(const char *path)
{
    if (!path)
        return -1;

    char dir[PATH_MAX];
    if (snprintf_checked(dir, sizeof(dir), "%s", path) != 0)
        return -1;

    char *slash = strrchr(dir, '/');
    if (!slash)
        return 0;

    *slash = '\0';
    return mkdir_p(dir, 0755);
}
