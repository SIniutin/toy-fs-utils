#ifndef ARCHIVE_FILE_OPS_H
#define ARCHIVE_FILE_OPS_H

#include <stddef.h>
#include <sys/stat.h>

void archive_dirname(const char *path, char *buf, size_t buflen);
int archive_write_text_file_atomic(const char *path, const char *text, const char *tool);
int archive_copy_file(const char *src, const char *dst, const struct stat *src_st, const char *tool);
int archive_read_source_path(const char *backup_root, char *out, size_t out_sz);

#endif
