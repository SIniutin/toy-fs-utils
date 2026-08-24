#define _XOPEN_SOURCE 700

#include "archive/paths.h"
#include "utils/common.h"

#include <limits.h>
#include <stdlib.h>

int archive_backups_dir(char *out, size_t outsz)
{
    const char *home = getenv("HOME");
    if (!home)
        return -1;
    return join_path_into(out, outsz, home, "Backups");
}

int archive_backup_dir(char *out, size_t outsz, const char *backup_name)
{
    char backups_dir[PATH_MAX];
    if (archive_backups_dir(backups_dir, sizeof(backups_dir)) != 0)
        return -1;
    return join_path_into(out, outsz, backups_dir, backup_name);
}

int archive_versions_dir(char *out, size_t outsz, const char *backup_root)
{
    return join_path_into(out, outsz, backup_root, ".versions");
}

int archive_source_path_file(char *out, size_t outsz, const char *backup_root)
{
    return join_path_into(out, outsz, backup_root, ".source_path");
}
