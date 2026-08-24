#ifndef ARCHIVE_RESTORE_H
#define ARCHIVE_RESTORE_H

#include <stddef.h>

int archive_restore_should_skip_rel(const char *rel);
int archive_restore_resolve_dest_root(const char *backup_root, const char *to_dir, char *out, size_t outsz);
int archive_restore_dest_path(const char *dest_root, const char *rel, char *out, size_t outsz);
int archive_restore_ensure_parent_dir(const char *path);

#endif
