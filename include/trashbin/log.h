#ifndef TRASHBIN_LOG_H
#define TRASHBIN_LOG_H

#include <limits.h>

typedef struct
{
    char original[PATH_MAX];
    char link[PATH_MAX];
    unsigned long inode;
    long size;
    char timestamp[64];
} trash_log_entry_t;

int trash_log_parse_line(char *line, trash_log_entry_t *out);

#endif
