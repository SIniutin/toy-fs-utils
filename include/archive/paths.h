#ifndef ARCHIVE_PATHS_H
#define ARCHIVE_PATHS_H

#include <stddef.h>

int archive_backups_dir(char *out, size_t outsz);
int archive_backup_dir(char *out, size_t outsz, const char *backup_name);
int archive_versions_dir(char *out, size_t outsz, const char *backup_root);
int archive_source_path_file(char *out, size_t outsz, const char *backup_root);

#endif
