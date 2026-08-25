#define _XOPEN_SOURCE 700

#include "trashbin/paths.h"
#include "utils/common.h"

#include <stdlib.h>

int trash_dir_path(char *out, size_t outsz)
{
    const char *home = getenv("HOME");
    if (!home)
        return -1;
    return join_path_into(out, outsz, home, ".trash");
}

int trash_log_path(char *out, size_t outsz)
{
    const char *home = getenv("HOME");
    if (!home)
        return -1;
    return join_path_into(out, outsz, home, ".trash.log");
}
