#define _XOPEN_SOURCE 700

#include "utils/common.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// char *get_current_datetime()
// {
//     static char buffer[20];
//     time_t rawtime;
//     struct tm *timeinfo;

//     time(&rawtime);
//     timeinfo = localtime(&rawtime);

//     strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeinfo);
//     return buffer;
// }

// void get_current_datetime(char *buffer, size_t sz)
// {
//     time_t rawtime = time(NULL);
//     struct tm *timeinfo = localtime(&rawtime);
//     if (!timeinfo) {
//         if (sz > 0) buffer[0] = '\0';
//         return;
//     }
//     strftime(buffer, sz, "%Y-%m-%d %H:%M", timeinfo);
// }

void get_current_datetime(char *buffer, size_t sz)
{
    if (!buffer || sz == 0)
        return;

    time_t rawtime = time(NULL);
    struct tm tm_buf;
    struct tm *timeinfo = localtime_r(&rawtime, &tm_buf);
    if (!timeinfo)
    {
        buffer[0] = '\0';
        return;
    }

    // РОВНО тот формат, который требуют тесты:
    // YYYY-mm-dd_HH-MM-SS
    strftime(buffer, sz, "%Y-%m-%d_%H-%M-%S", timeinfo);
}

char *join_path(const char *path, const char *name)
{
    size_t path_len = strlen(path);
    size_t name_len = strlen(name);
    char *new_path = malloc(path_len + name_len + 2);
    if (!new_path)
        return NULL;

    strcpy(new_path, path);
    strcat(new_path, "/");
    strcat(new_path, name);
    return new_path;
}

int snprintf_checked(char *out, size_t outsz, const char *fmt, ...)
{
    if (!out || outsz == 0 || !fmt)
        return -1;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, outsz, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= outsz)
    {
        out[0] = '\0';
        return -1;
    }
    return 0;
}

int join_path_into(char *out, size_t outsz, const char *path, const char *name)
{
    return snprintf_checked(out, outsz, "%s/%s", path, name);
}

const char *get_basename(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

static int ensure_dir(const char *path, mode_t mode)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (!S_ISDIR(st.st_mode))
        {
            // fprintf(stderr, "upback: %s exists and is not a directory\n", path);
            return -1;
        }
        return 0;
    }
    if (mkdir(path, mode) != 0)
    {
        if (errno == EEXIST)
            return 0;
        // perror("upback: mkdir");
        return -1;
    }
    return 0;
}

int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return -1;
    strcpy(tmp, path);

    for (char *p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (ensure_dir(tmp, mode) != 0)
                return -1;
            *p = '/';
        }
    }
    if (ensure_dir(tmp, mode) != 0)
        return -1;
    return 0;
}
