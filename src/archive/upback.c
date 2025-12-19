#define _XOPEN_SOURCE 700

#include "utils/common.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

static int read_source_path(const char *backup_root, char *out, size_t out_sz)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/.source_path", backup_root);

    FILE *f = fopen(p, "r");
    if (!f)
        return -1;

    char line[PATH_MAX + 32];
    if (!fgets(line, sizeof(line), f))
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    size_t n = strcspn(line, "\r\n");
    line[n] = '\0';

    const char *val = line;

    if (strncmp(line, "SOURCE=", 7) == 0)
        val = line + 7;

    if (val[0] == '\0')
        return -1;

    if (snprintf(out, out_sz, "%s", val) >= (int)out_sz)
        return -1;
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    int in = open(src, O_RDONLY);
    if (in < 0)
    {
        fprintf(stderr, "upback: cannot open %s: %s\n", src, strerror(errno));
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0)
    {
        fprintf(stderr, "upback: cannot open %s: %s\n", dst, strerror(errno));
        close(in);
        return -1;
    }

    char buf[65536];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0)
    {
        ssize_t off = 0;
        while (off < r)
        {
            ssize_t w = write(out, buf + off, (size_t)(r - off));
            if (w < 0)
            {
                fprintf(stderr, "upback: write error: %s\n", strerror(errno));
                close(in);
                close(out);
                return -1;
            }
            off += w;
        }
    }
    if (r < 0)
    {
        fprintf(stderr, "upback: read error: %s\n", strerror(errno));
        close(in);
        close(out);
        return -1;
    }

    close(in);
    close(out);
    return 0;
}

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
    if (strncmp(rel, ".versions", 9) == 0 && (rel[9] == '/' || rel[9] == '\0'))
    {
        return 0;
    }
    if (strcmp(rel, "versions.tar.gz") == 0)
        return 0;
    if (strncmp(rel, ".versions/", 10) == 0)
        return 0;

    if (*rel == '\0')
    {
        return 0;
    }

    char dest_path[PATH_MAX];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", g_ctx.dest_root, rel);

    char dest_dir[PATH_MAX];
    snprintf(dest_dir, sizeof(dest_dir), "%s", dest_path);
    char *slash = strrchr(dest_dir, '/');
    if (slash)
    {
        *slash = '\0';
        if (mkdir_p(dest_dir, 0755) != 0)
        {
            fprintf(stderr, "upback: failed to create dir %s\n", dest_dir);
            return 0;
        }
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

    if (copy_file(fpath, dest_path) == 0)
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
            snprintf(to_dir, sizeof(to_dir), "%s", argv[++i]);
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
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/Backups/%s", home, version);
    if (!realpath(tmp, backup_root))
    {
        return 1;
    }

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
        snprintf(backup_root, sizeof(backup_root), "%s/Backups/%s", home, version);
    }

    struct stat st_bak;
    if (stat(backup_root, &st_bak) != 0 || !S_ISDIR(st_bak.st_mode))
    {
        fprintf(stderr, "upback: backup dir not found: %s\n", backup_root);
        return 1;
    }

    char dest_root[PATH_MAX];

    if (to_dir[0])
    {
        if (!realpath(to_dir, dest_root))
        {
            snprintf(dest_root, sizeof(dest_root), "%s", to_dir);
            if (mkdir_p(dest_root, 0755) != 0)
            {
                fprintf(stderr, "upback: cannot create dest dir %s\n", dest_root);
                return 1;
            }
            if (!realpath(dest_root, dest_root))
            {
                fprintf(stderr, "upback: cannot resolve dest dir %s\n", dest_root);
                return 1;
            }
        }
    }
    else
    {
        char src_restore[PATH_MAX];
        if (read_source_path(backup_root, src_restore, sizeof(src_restore)) != 0)
        {
            fprintf(stderr, "upback: no .source_path in backup, use --to DIR\n");
            return 1;
        }

        if (!realpath(src_restore, dest_root))
        {
            if (mkdir_p(src_restore, 0755) != 0 || !realpath(src_restore, dest_root))
            {
                fprintf(stderr, "upback: cannot resolve/create source dir %s\n", src_restore);
                return 1;
            }
        }
        if (mkdir_p(dest_root, 0755) != 0)
        {
            fprintf(stderr, "upback: cannot create dest root %s\n", dest_root);
            return 1;
        }
        if (!realpath(dest_root, dest_root))
        {
            fprintf(stderr, "upback: cannot resolve dest root %s\n", dest_root);
            return 1;
        }
    }

    printf("Restoring from: %s\n", backup_root);
    printf("Destination root: %s\n", dest_root);

    memset(&g_ctx, 0, sizeof(g_ctx));
    snprintf(g_ctx.backup_root, sizeof(g_ctx.backup_root), "%s", backup_root);
    g_ctx.backup_root_len = strlen(g_ctx.backup_root);
    snprintf(g_ctx.dest_root, sizeof(g_ctx.dest_root), "%s", dest_root);

    if (nftw(g_ctx.backup_root, nftw_cb, 16, FTW_PHYS) != 0)
    {
        fprintf(stderr, "upback: nftw failed\n");
        return 1;
    }

    return 0;
}
