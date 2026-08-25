#define _XOPEN_SOURCE 700

#include "archive/file_ops.h"
#include "archive/paths.h"
#include "utils/common.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

void archive_dirname(const char *path, char *buf, size_t buflen)
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

int archive_write_text_file_atomic(const char *path, const char *text, const char *tool)
{
    char tmp[PATH_MAX];
    if (snprintf_checked(tmp, sizeof(tmp), "%s.tmp", path) != 0)
    {
        fprintf(stderr, "%s: temporary path is too long: %s.tmp\n", tool, path);
        return -1;
    }

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "%s: cannot open %s: %s\n", tool, tmp, strerror(errno));
        return -1;
    }

    size_t n = strlen(text);
    ssize_t w = write(fd, text, n);
    if (w < 0 || (size_t)w != n)
    {
        fprintf(stderr, "%s: write error %s: %s\n", tool, tmp, strerror(errno));
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (close(fd) != 0)
    {
        fprintf(stderr, "%s: close error %s: %s\n", tool, tmp, strerror(errno));
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0)
    {
        fprintf(stderr, "%s: rename(%s -> %s) failed: %s\n", tool, tmp, path, strerror(errno));
        unlink(tmp);
        return -1;
    }

    return 0;
}

int archive_copy_file(const char *src, const char *dst, const struct stat *src_st, const char *tool)
{
    int in = open(src, O_RDONLY);
    if (in < 0)
    {
        fprintf(stderr, "%s: cannot open %s: %s\n", tool, src, strerror(errno));
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0)
    {
        fprintf(stderr, "%s: cannot open %s: %s\n", tool, dst, strerror(errno));
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
                fprintf(stderr, "%s: write error: %s\n", tool, strerror(errno));
                close(in);
                close(out);
                return -1;
            }
            off += w;
        }
    }
    if (r < 0)
    {
        fprintf(stderr, "%s: read error: %s\n", tool, strerror(errno));
        close(in);
        close(out);
        return -1;
    }

    close(in);
    close(out);

    if (src_st)
    {
        struct utimbuf ut;
        ut.actime = src_st->st_atime;
        ut.modtime = src_st->st_mtime;
        if (utime(dst, &ut) != 0)
        {
            fprintf(stderr, "%s: utime(%s) failed: %s\n", tool, dst, strerror(errno));
            return -1;
        }
    }

    return 0;
}

int archive_read_source_path(const char *backup_root, char *out, size_t out_sz)
{
    char p[PATH_MAX];
    if (archive_source_path_file(p, sizeof(p), backup_root) != 0)
        return -1;

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

    return snprintf_checked(out, out_sz, "%s", val);
}
