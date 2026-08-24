#define _XOPEN_SOURCE 700

#include "trashbin/log.h"
#include "trashbin/paths.h"
#include "trashbin/restore.h"
#include "utils/common.h"
#include <sys/stat.h>

#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNTRASH_OK 0
#define UNTRASH_USAGE 1
#define UNTRASH_IO 2
#define UNTRASH_INTERNAL 3

static int ask_yes_no(void)
{
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin))
    {
        return 0;
    }
    return (buf[0] == 'y' || buf[0] == 'Y');
}

int main(int argc, char *argv[])
{
    int mode_overwrite = 0;
    int mode_unique = 0;
    char to_dir[PATH_MAX] = "";
    const char *pattern = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--overwrite") == 0)
        {
            mode_overwrite = 1;
        }
        else if (strcmp(argv[i], "--unique") == 0)
        {
            mode_unique = 1;
        }
        else if (strcmp(argv[i], "--to") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "untrash: --to requires directory\n");
                return UNTRASH_USAGE;
            }
            if (snprintf_checked(to_dir, sizeof(to_dir), "%s", argv[++i]) != 0)
            {
                fprintf(stderr, "untrash: --to directory is too long\n");
                return UNTRASH_USAGE;
            }
        }
        else
        {
            if (!pattern)
            {
                pattern = argv[i];
            }
            else
            {
                fprintf(stderr, "untrash: unexpected extra argument: %s\n", argv[i]);
                return UNTRASH_USAGE;
            }
        }
    }

    if (!pattern)
    {
        fprintf(stderr, "Usage: untrash [--overwrite|--unique] [--to DIR] PATTERN\n");
        return UNTRASH_USAGE;
    }
    if (mode_overwrite && mode_unique)
    {
        fprintf(stderr, "untrash: --overwrite and --unique are mutually exclusive\n");
        return UNTRASH_USAGE;
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "untrash: HOME is not set\n");
        return UNTRASH_USAGE;
    }

    char log_path[PATH_MAX];
    char trash_dir[PATH_MAX];
    if (trash_log_path(log_path, sizeof(log_path)) != 0 ||
        trash_dir_path(trash_dir, sizeof(trash_dir)) != 0)
    {
        fprintf(stderr, "untrash: HOME path is too long\n");
        return UNTRASH_USAGE;
    }

    FILE *log = fopen(log_path, "r");
    if (!log)
    {
        int open_errno = errno;
        if (open_errno == ENOENT)
        {
            fprintf(stderr, "untrash: trash is empty\n");
            return UNTRASH_OK;
        }
        perror("untrash: open ~/.trash.log");
        return UNTRASH_IO;
    }

    struct stat st_trash;
    if (stat(trash_dir, &st_trash) != 0 || !S_ISDIR(st_trash.st_mode))
    {
        fprintf(stderr, "untrash: trash dir not found: %s\n", trash_dir);
        fclose(log);
        return UNTRASH_OK;
    }

    int exit_status = UNTRASH_OK;
    char line[4096];
    while (fgets(line, sizeof(line), log))
    {
        trash_log_entry_t e;
        if (trash_log_parse_line(line, &e) != 0)
            continue;

        const char *base = get_basename(e.original);
        if (fnmatch(pattern, base, 0) != 0)
        {
            continue;
        }

        char when[64];
        trash_restore_format_deleted_time(e.timestamp, when, sizeof(when));

        printf("Found: %s (deleted at %s)\n", e.original, when);
        printf("Restore? [y/n] ");
        fflush(stdout);

        if (!ask_yes_no())
        {
            continue;
        }

        char dest_dir[PATH_MAX];
        if (trash_restore_prepare_dest_dir(e.original, to_dir, home, dest_dir, sizeof(dest_dir)) != 0)
        {
            fprintf(stderr, "untrash: failed to prepare destination directory\n");
            exit_status = UNTRASH_IO;
            continue;
        }

        const char *orig_name = base;
        char dest_path[PATH_MAX];

        int dest_status =
            trash_restore_prepare_dest_path(dest_dir, orig_name, mode_overwrite, mode_unique, dest_path, sizeof(dest_path));
        if (dest_status == TRASH_RESTORE_DEST_EXISTS)
        {
            fprintf(stderr, "untrash: destination exists, skipping: %s\n", dest_path);
            exit_status = UNTRASH_USAGE;
            continue;
        }
        if (dest_status != TRASH_RESTORE_DEST_OK)
        {
            fprintf(stderr, "untrash: failed to prepare destination path: %s/%s\n", dest_dir, orig_name);
            exit_status = UNTRASH_IO;
            continue;
        }

        char src_path[PATH_MAX];
        if (join_path_into(src_path, sizeof(src_path), trash_dir, e.link) != 0)
        {
            fprintf(stderr, "untrash: trash source path is too long: %s/%s\n", trash_dir, e.link);
            exit_status = UNTRASH_IO;
            continue;
        }

        if (access(src_path, F_OK) != 0)
        {
            fprintf(stderr, "untrash: source in trash not found: %s\n", src_path);
            exit_status = UNTRASH_IO;
            continue;
        }

        if (trash_restore_file(src_path, dest_path) == 0)
        {
            printf("Restored to: %s\n", dest_path);
        }
        else
        {
            fprintf(stderr, "untrash: failed to restore %s\n", e.original);
            exit_status = UNTRASH_IO;
        }
    }

    if (ferror(log))
    {
        perror("untrash: read ~/.trash.log");
        exit_status = UNTRASH_IO;
    }

    fclose(log);
    return exit_status;
}
