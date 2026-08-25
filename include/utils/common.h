#ifndef UTILS_COMMON_H
#define UTILS_COMMON_H

#include <sys/types.h>

#define _XOPEN_SOURCE 700

void get_current_datetime(char *buffer, size_t sz);
// char *get_current_datetime();
char *join_path(const char *path, const char *name);
int snprintf_checked(char *out, size_t outsz, const char *fmt, ...);
int join_path_into(char *out, size_t outsz, const char *path, const char *name);
const char *get_basename(const char *path);
int mkdir_p(const char *path, mode_t mode);

#endif
