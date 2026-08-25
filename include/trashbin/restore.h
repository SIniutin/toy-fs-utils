#ifndef TRASHBIN_RESTORE_H
#define TRASHBIN_RESTORE_H

#include <stddef.h>

#define TRASH_RESTORE_DEST_OK 0
#define TRASH_RESTORE_DEST_EXISTS 1
#define TRASH_RESTORE_DEST_ERROR -1

void trash_restore_format_deleted_time(const char *ts, char *buf, size_t buflen);
void trash_restore_get_dirname(const char *path, char *buf, size_t buflen);
int trash_restore_file(const char *src, const char *dst);
int trash_restore_make_unique_name(const char *dir, const char *name, char *out, size_t outsz);
int trash_restore_prepare_dest_dir(const char *original, const char *to_dir, const char *home, char *out, size_t outsz);
int trash_restore_prepare_dest_path(const char *dest_dir, const char *orig_name, int overwrite, int unique, char *out,
                                    size_t outsz);

#endif
