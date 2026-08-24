#define _XOPEN_SOURCE 700

#include "archive/file_ops.h"
#include "archive/paths.h"
#include "archive/restore.h"
#include "utils/common.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    char backup_root[PATH_MAX];
    size_t backup_root_len;

    char dest_root[PATH_MAX];
} up_ctx_t;

static up_ctx_t g_ctx;

static int ask_confirm(void)
{
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin))
    {
        return 0;
    }
    return (buf[0] == 'y' || buf[0] == 'Y');
}

static int nftw_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;

    if (typeflag != FTW_F)
        return 0;

    const char *rel = fpath + g_ctx.backup_root_len;
    if (*rel == '/')
        rel++;
    if (archive_restore_should_skip_rel(rel))
    {
        return 0;
    }

    char dest_path[PATH_MAX];
    if (archive_restore_dest_path(g_ctx.dest_root, rel, dest_path, sizeof(dest_path)) != 0)
    {
        fprintf(stderr, "upback: destination path is too long: %s/%s\n", g_ctx.dest_root, rel);
        return 0;
    }

    if (archive_restore_ensure_parent_dir(dest_path) != 0)
    {
        fprintf(stderr, "upback: failed to create parent dir for %s\n", dest_path);
        return 0;
    }

    struct stat st_dest;
    int dest_exists = (stat(dest_path, &st_dest) == 0);

    if (dest_exists)
    {
        printf("Target exists: %s\nOverwrite? [y/n] ", dest_path);
        fflush(stdout);
        if (!ask_confirm())
        {
            printf("Skipped: %s\n", dest_path);
            return 0;
        }
    }

    if (archive_copy_file(fpath, dest_path, NULL, "upback") == 0)
    {
        printf("Restored: %s\n", dest_path);
    }
    else
    {
        fprintf(stderr, "upback: failed to restore %s\n", dest_path);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    char to_dir[PATH_MAX] = "";
    const char *version = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--to") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Usage: upback [--to DIR] BACKUP_NAME\n");
                return 1;
            }
            if (snprintf_checked(to_dir, sizeof(to_dir), "%s", argv[++i]) != 0)
            {
                fprintf(stderr, "upback: destination argument is too long\n");
                return 1;
            }
        }
        else
        {
            if (version)
            {
                fprintf(stderr, "Usage: upback [--to DIR] BACKUP_NAME\n");
                return 1;
            }
            version = argv[i];
        }
    }

    if (!version)
    {
        fprintf(stderr, "Usage: upback [--to DIR] BACKUP_NAME\n");
        return 1;
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "upback: HOME is not set\n");
        return 1;
    }

    char backup_root[PATH_MAX];
    if (strchr(version, '/'))
    {
        if (!realpath(version, backup_root))
        {
            fprintf(stderr, "upback: cannot resolve %s: %s\n", version, strerror(errno));
            return 1;
        }
    }
    else
    {
        char backups_dir[PATH_MAX];
        if (archive_backups_dir(backups_dir, sizeof(backups_dir)) != 0 ||
            join_path_into(backup_root, sizeof(backup_root), backups_dir, version) != 0)
        {
            fprintf(stderr, "upback: backup path is too long\n");
            return 1;
        }
    }

    struct stat st_bak;
    if (stat(backup_root, &st_bak) != 0 || !S_ISDIR(st_bak.st_mode))
    {
        fprintf(stderr, "upback: backup dir not found: %s\n", backup_root);
        return 1;
    }

    char dest_root[PATH_MAX];

    if (archive_restore_resolve_dest_root(backup_root, to_dir, dest_root, sizeof(dest_root)) != 0)
    {
        if (!to_dir[0])
            fprintf(stderr, "upback: no .source_path in backup, use --to DIR\n");
        else
            fprintf(stderr, "upback: cannot resolve/create dest dir %s\n", to_dir);
        return 1;
    }

    printf("Restoring from: %s\n", backup_root);
    printf("Destination root: %s\n", dest_root);

    memset(&g_ctx, 0, sizeof(g_ctx));
    if (snprintf_checked(g_ctx.backup_root, sizeof(g_ctx.backup_root), "%s", backup_root) != 0)
    {
        fprintf(stderr, "upback: backup path is too long\n");
        return 1;
    }
    g_ctx.backup_root_len = strlen(g_ctx.backup_root);
    if (snprintf_checked(g_ctx.dest_root, sizeof(g_ctx.dest_root), "%s", dest_root) != 0)
    {
        fprintf(stderr, "upback: destination path is too long\n");
        return 1;
    }

    if (nftw(g_ctx.backup_root, nftw_cb, 16, FTW_PHYS) != 0)
    {
        fprintf(stderr, "upback: nftw failed\n");
        return 1;
    }

    return 0;
}
