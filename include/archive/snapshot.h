#ifndef ARCHIVE_SNAPSHOT_H
#define ARCHIVE_SNAPSHOT_H

#include <stddef.h>
#include <sys/stat.h>

typedef enum
{
    ARCHIVE_FILE_NEW,
    ARCHIVE_FILE_CHANGED,
    ARCHIVE_FILE_UNCHANGED
} archive_file_status_t;

int archive_file_is_unchanged(const struct stat *orig, const struct stat *bak);
archive_file_status_t archive_compare_with_dst(const struct stat *orig, const char *dst, const char *tool);
int archive_version_path(const char *backup_root, const char *rel, const char *datetime, char *out, size_t outsz);

#endif
