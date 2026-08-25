#define _XOPEN_SOURCE 700

#include "trashbin/log.h"
#include "utils/common.h"

#include <stdlib.h>
#include <string.h>

static char *trim_field(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;

    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
        *--end = '\0';

    return s;
}

int trash_log_parse_line(char *line, trash_log_entry_t *out)
{
    if (!line || !out)
        return -1;

    trash_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    char *p = strtok(line, "|");
    if (!p || snprintf_checked(entry.original, sizeof(entry.original), "%s", trim_field(p)) != 0)
        return -1;

    p = strtok(NULL, "|");
    if (!p || snprintf_checked(entry.link, sizeof(entry.link), "%s", trim_field(p)) != 0)
        return -1;

    p = strtok(NULL, "|");
    if (!p)
        return -1;
    entry.inode = strtoul(trim_field(p), NULL, 10);

    p = strtok(NULL, "|");
    if (!p)
        return -1;
    entry.size = atol(trim_field(p));

    p = strtok(NULL, "|");
    if (!p || snprintf_checked(entry.timestamp, sizeof(entry.timestamp), "%s", trim_field(p)) != 0)
        return -1;

    *out = entry;
    return 0;
}
