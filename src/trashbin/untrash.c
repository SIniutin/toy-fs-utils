#define _XOPEN_SOURCE 700

#include "utils/common.h"
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    char original[PATH_MAX];
    char link[PATH_MAX];
    unsigned long inode;
    long size;
    char timestamp[32];
} log_entry_t;

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static void format_deleted_time(const char *ts, char *buf, size_t buflen)
{
    if (!ts || strlen(ts) < 14)
    {
        snprintf(buf, buflen, "%s", ts ? ts : "?");
        return;
    }
    snprintf(buf, buflen, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s", ts, ts + 4, ts + 6, ts + 8, ts + 10, ts + 12);
}

static void get_dirname(const char *path, char *buf, size_t buflen)
{
    const char *p = strrchr(path, '/');
    if (!p)
    {
        // нет слеша — считаем текущую директорию
        snprintf(buf, buflen, ".");
        return;
    }
    size_t len = (size_t)(p - path);
    if (len >= buflen)
        len = buflen - 1;
    memcpy(buf, path, len);
    buf[len] = '\0';
}

static int restore_file(const char *src, const char *dst)
{
    if (rename(src, dst) == 0)
    {
        return 0;
    }
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

static void make_unique_name(const char *dir, const char *name, char *out, size_t outsz)
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
        snprintf(candidate_name, sizeof(candidate_name), "%s(%d)%s", base, n, ext);

        snprintf(out, outsz, "%s/%s", dir, candidate_name);

        struct stat st;
        if (stat(out, &st) != 0)
        {
            return;
        }
        n++;
    }
}

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
                return 1;
            }
            snprintf(to_dir, sizeof(to_dir), "%s", argv[++i]);
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
                return 1;
            }
        }
    }

    if (!pattern)
    {
        fprintf(stderr, "Usage: untrash [--overwrite|--unique] [--to DIR] PATTERN\n");
        return 1;
    }
    if (mode_overwrite && mode_unique)
    {
        fprintf(stderr, "untrash: --overwrite and --unique are mutually exclusive\n");
        return 1;
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "untrash: HOME is not set\n");
        return 1;
    }

    char log_path[PATH_MAX];
    char trash_dir[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/.trash.log", home);
    snprintf(trash_dir, sizeof(trash_dir), "%s/.trash", home);

    FILE *log = fopen(log_path, "r");
    if (!log)
    {
        perror("untrash: open ~/.trash.log");
        return 1;
    }

    struct stat st_trash;
    if (stat(trash_dir, &st_trash) != 0 || !S_ISDIR(st_trash.st_mode))
    {
        fprintf(stderr, "untrash: trash dir not found: %s\n", trash_dir);
        fclose(log);
        return 1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), log))
    {
        char *p = strtok(line, "|");
        if (!p)
            continue;

        log_entry_t e;
        memset(&e, 0, sizeof(e));

        snprintf(e.original, sizeof(e.original), "%s", trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        snprintf(e.link, sizeof(e.link), "%s", trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        e.inode = strtoul(trim(p), NULL, 10);

        p = strtok(NULL, "|");
        if (!p)
            continue;
        e.size = atol(trim(p));

        p = strtok(NULL, "|");
        if (!p)
            continue;
        snprintf(e.timestamp, sizeof(e.timestamp), "%s", trim(p));

        const char *base = get_basename(e.original);
        if (fnmatch(pattern, base, 0) != 0)
        {
            continue;
        }

        char when[64];
        format_deleted_time(e.timestamp, when, sizeof(when));

        printf("Found: %s (deleted at %s)\n", e.original, when);
        printf("Restore? [y/n] ");
        fflush(stdout);

        if (!ask_yes_no())
        {
            continue;
        }

        char dest_dir[PATH_MAX];
        if (to_dir[0])
        {
            snprintf(dest_dir, sizeof(dest_dir), "%s", to_dir);
            if (mkdir_p(dest_dir, 0755) != 0)
            {
                fprintf(stderr, "untrash: failed to create directory %s\n", dest_dir);
                continue;
            }
        }
        else
        {
            char orig_dir[PATH_MAX];
            get_dirname(e.original, orig_dir, sizeof(orig_dir));

            struct stat st_dir;
            if (stat(orig_dir, &st_dir) == 0 && S_ISDIR(st_dir.st_mode))
            {
                snprintf(dest_dir, sizeof(dest_dir), "%s", orig_dir);
            }
            else
            {
                snprintf(dest_dir, sizeof(dest_dir), "%s/restore-lost", home);
                if (mkdir_p(dest_dir, 0700) != 0)
                {
                    fprintf(stderr, "untrash: failed to create restore-lost dir %s\n", dest_dir);
                    continue;
                }
            }
        }

        const char *orig_name = base;
        char dest_path[PATH_MAX];

        struct stat st_dst;

        if (mode_unique)
        {
            make_unique_name(dest_dir, orig_name, dest_path, sizeof(dest_path));
        }
        else
        {
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, orig_name);

            if (stat(dest_path, &st_dst) == 0)
            {
                if (mode_overwrite)
                {
                    if (unlink(dest_path) != 0)
                    {
                        perror("untrash: unlink existing dest");
                        continue;
                    }
                }
                else
                {
                    fprintf(stderr, "untrash: destination exists, skipping: %s\n", dest_path);
                    continue;
                }
            }
        }

        char src_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", trash_dir, e.link);

        if (access(src_path, F_OK) != 0)
        {
            fprintf(stderr, "untrash: source in trash not found: %s\n", src_path);
            continue;
        }

        if (restore_file(src_path, dest_path) == 0)
        {
            printf("Restored to: %s\n", dest_path);
        }
        else
        {
            fprintf(stderr, "untrash: failed to restore %s\n", e.original);
        }
    }

    fclose(log);
    return 0;
}
