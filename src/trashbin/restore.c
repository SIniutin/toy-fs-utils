#define _XOPEN_SOURCE 700

#include "trashbin/restore.h"
#include "utils/common.h"

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void trash_restore_format_deleted_time(const char *ts, char *buf, size_t buflen)
{
    if (!ts)
    {
        snprintf(buf, buflen, "?");
        return;
    }

    if (strlen(ts) >= 19 && ts[4] == '-' && ts[7] == '-' && ts[10] == '_' && ts[13] == '-' && ts[16] == '-')
    {
        snprintf(buf, buflen, "%.10s %.2s:%.2s:%.2s", ts, ts + 11, ts + 14, ts + 17);
        return;
    }

    if (strlen(ts) >= 14)
    {
        snprintf(buf, buflen, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s", ts, ts + 4, ts + 6, ts + 8, ts + 10, ts + 12);
        return;
    }

    snprintf(buf, buflen, "%s", ts);
}

void trash_restore_get_dirname(const char *path, char *buf, size_t buflen)
{
    const char *p = strrchr(path, '/');
    if (!p)
    {
        snprintf(buf, buflen, ".");
        return;
    }

    size_t len = (size_t)(p - path);
    if (len >= buflen)
        len = buflen - 1;
    memcpy(buf, path, len);
    buf[len] = '\0';
}

int trash_restore_file(const char *src, const char *dst)
{
    if (rename(src, dst) == 0)
        return 0;

    if (errno != EXDEV)
    {
        fprintf(stderr, "untrash: rename %s -> %s failed: %s\n", src, dst, strerror(errno));
        return -1;
    }

    int in = open(src, O_RDONLY);
    if (in < 0)
    {
        perror("untrash: open src");
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0)
    {
        perror("untrash: open dst");
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
                perror("untrash: write");
                close(in);
                close(out);
                return -1;
            }
            off += w;
        }
    }
    if (r < 0)
    {
        perror("untrash: read");
        close(in);
        close(out);
        return -1;
    }

    close(in);
    close(out);

    if (unlink(src) != 0)
    {
        perror("untrash: unlink src");
    }

    return 0;
}

int trash_restore_make_unique_name(const char *dir, const char *name, char *out, size_t outsz)
{
    const char *dot = strrchr(name, '.');
    char base[PATH_MAX];
    char ext[PATH_MAX];

    if (dot && dot != name)
    {
        size_t blen = (size_t)(dot - name);
        if (blen >= sizeof(base))
            blen = sizeof(base) - 1;
        memcpy(base, name, blen);
        base[blen] = '\0';
        snprintf(ext, sizeof(ext), "%s", dot);
    }
    else
    {
        snprintf(base, sizeof(base), "%s", name);
        ext[0] = '\0';
    }

    int n = 1;
    for (;;)
    {
        char candidate_name[PATH_MAX];
        if (snprintf_checked(candidate_name, sizeof(candidate_name), "%s(%d)%s", base, n, ext) != 0)
            return -1;

        if (join_path_into(out, outsz, dir, candidate_name) != 0)
            return -1;

        struct stat st;
        if (stat(out, &st) != 0)
        {
            return 0;
        }
        n++;
    }
}

int trash_restore_prepare_dest_dir(const char *original, const char *to_dir, const char *home, char *out, size_t outsz)
{
    if (!original || !home || !out)
        return -1;

    if (to_dir && to_dir[0])
    {
        if (snprintf_checked(out, outsz, "%s", to_dir) != 0)
            return -1;
        return mkdir_p(out, 0755);
    }

    char orig_dir[PATH_MAX];
    trash_restore_get_dirname(original, orig_dir, sizeof(orig_dir));

    struct stat st_dir;
    if (stat(orig_dir, &st_dir) == 0 && S_ISDIR(st_dir.st_mode))
        return snprintf_checked(out, outsz, "%s", orig_dir);

    if (join_path_into(out, outsz, home, "restore-lost") != 0)
        return -1;
    return mkdir_p(out, 0700);
}

int trash_restore_prepare_dest_path(const char *dest_dir, const char *orig_name, int overwrite, int unique, char *out,
                                    size_t outsz)
{
    if (!dest_dir || !orig_name || !out)
        return TRASH_RESTORE_DEST_ERROR;

    if (unique)
    {
        if (trash_restore_make_unique_name(dest_dir, orig_name, out, outsz) != 0)
            return TRASH_RESTORE_DEST_ERROR;
        return TRASH_RESTORE_DEST_OK;
    }

    if (join_path_into(out, outsz, dest_dir, orig_name) != 0)
        return TRASH_RESTORE_DEST_ERROR;

    struct stat st_dst;
    if (stat(out, &st_dst) == 0)
    {
        if (!overwrite)
            return TRASH_RESTORE_DEST_EXISTS;
        if (unlink(out) != 0)
            return TRASH_RESTORE_DEST_ERROR;
    }

    return TRASH_RESTORE_DEST_OK;
}
