#define _XOPEN_SOURCE 700

#include "utils/common.h"
#include "utils/crawler.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

typedef struct
{
    char **items;
    size_t len;
    size_t cap;
} strlist_t;

static void strlist_init(strlist_t *l)
{
    l->items = NULL;
    l->len = 0;
    l->cap = 0;
}

static void strlist_push(strlist_t *l, const char *s)
{
    if (l->len == l->cap)
    {
        size_t ncap = l->cap ? l->cap * 2 : 16;
        char **n = realloc(l->items, ncap * sizeof(char *));
        if (!n)
        {
            fprintf(stderr, "backup: realloc failed\n");
            exit(1);
        }
        l->items = n;
        l->cap = ncap;
    }
    l->items[l->len] = strdup(s);
    if (!l->items[l->len])
    {
        fprintf(stderr, "backup: strdup failed\n");
        exit(1);
    }
    l->len++;
}

static void strlist_free(strlist_t *l)
{
    for (size_t i = 0; i < l->len; i++)
        free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->len = l->cap = 0;
}

typedef struct
{
    char src_root[PATH_MAX];
    size_t src_root_len;

    char backup_root[PATH_MAX];
    char datetime[32];    // YYYY-MM-DD_HH-MM-SS

    int check_only;
    int compress;

    size_t cnt_new;
    size_t cnt_updated;
    size_t cnt_skipped;

    strlist_t new_files;
    strlist_t updated_files;
    strlist_t skipped_files;

    pthread_mutex_t lock;
} backup_ctx_t;

static void get_dirname(const char *path, char *buf, size_t buflen)
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
static int write_text_file_atomic(const char *path, const char *text)
{
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
        fprintf(stderr, "backup: cannot open %s: %s\n", tmp, strerror(errno));
        return -1;
    }

    size_t n = strlen(text);
    ssize_t w = write(fd, text, n);
    if (w < 0 || (size_t)w != n)
    {
        fprintf(stderr, "backup: write error %s: %s\n", tmp, strerror(errno));
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (close(fd) != 0)
    {
        fprintf(stderr, "backup: close error %s: %s\n", tmp, strerror(errno));
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0)
    {
        fprintf(stderr, "backup: rename(%s -> %s) failed: %s\n", tmp, path, strerror(errno));
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst, const struct stat *src_st)
{
    int in = open(src, O_RDONLY);
    if (in < 0)
    {
        fprintf(stderr, "backup: cannot open %s: %s\n", src, strerror(errno));
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0)
    {
        fprintf(stderr, "backup: cannot open %s: %s\n", dst, strerror(errno));
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
                fprintf(stderr, "backup: write error: %s\n", strerror(errno));
                close(in);
                close(out);
                return -1;
            }
            off += w;
        }
    }
    if (r < 0)
    {
        fprintf(stderr, "backup: read error: %s\n", strerror(errno));
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
            fprintf(stderr, "backup: utime(%s) failed: %s\n", dst, strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int is_unchanged(const struct stat *orig, const struct stat *bak)
{
    if (orig->st_ino == bak->st_ino && orig->st_dev == bak->st_dev)
        return 1;
    if (orig->st_size == bak->st_size && orig->st_mtime == bak->st_mtime)
        return 1;
    return 0;
}

typedef enum
{
    ST_NEW,
    ST_CHANGED,
    ST_UNCHANGED
} file_status_t;

static file_status_t compare_with_dst(const struct stat *orig, const char *dst)
{
    struct stat st_bak;
    if (stat(dst, &st_bak) != 0)
    {
        if (errno == ENOENT)
            return ST_NEW;
        fprintf(stderr, "backup: stat(%s) failed: %s\n", dst, strerror(errno));
        return ST_CHANGED;
    }
    return is_unchanged(orig, &st_bak) ? ST_UNCHANGED : ST_CHANGED;
}

static int process_file(const char *path, const struct stat *st, void *user_data)
{
    backup_ctx_t *ctx = user_data;

    if (!S_ISREG(st->st_mode))
        return 0;

    const char *rel = path + ctx->src_root_len;
    if (*rel == '/')
        rel++;

    char dst[PATH_MAX];
    snprintf(dst, sizeof(dst), "%s/%s", ctx->backup_root, rel);

    file_status_t stt = compare_with_dst(st, dst);

    if (ctx->check_only)
    {
        pthread_mutex_lock(&ctx->lock);
        switch (stt)
        {
        case ST_NEW:
            strlist_push(&ctx->new_files, rel);
            break;
        case ST_CHANGED:
            strlist_push(&ctx->updated_files, rel);
            break;
        case ST_UNCHANGED:
            strlist_push(&ctx->skipped_files, rel);
            break;
        }
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }

    if (stt == ST_NEW)
    {
        char dirbuf[PATH_MAX];
        get_dirname(dst, dirbuf, sizeof(dirbuf));
        if (mkdir_p(dirbuf, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", dirbuf);
            return -1;
        }

        if (copy_file(path, dst, st) != 0)
            return -1;

        pthread_mutex_lock(&ctx->lock);
        ctx->cnt_new++;
        strlist_push(&ctx->new_files, rel);
        pthread_mutex_unlock(&ctx->lock);
    }
    else if (stt == ST_CHANGED)
    {
        char versions_dir[PATH_MAX];
        snprintf(versions_dir, sizeof(versions_dir), "%s/.versions", ctx->backup_root);
        if (mkdir_p(versions_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", versions_dir);
            return -1;
        }

        char base_ver_path[PATH_MAX];
        snprintf(base_ver_path, sizeof(base_ver_path), "%s/%s@%s", versions_dir, rel, ctx->datetime);

        char ver_dir[PATH_MAX];
        get_dirname(base_ver_path, ver_dir, sizeof(ver_dir));
        if (mkdir_p(ver_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", ver_dir);
            return -1;
        }

        char ver_path[PATH_MAX];
        struct stat tmp;
        int idx = 0;
        for (;;)
        {
            if (idx == 0)
                snprintf(ver_path, sizeof(ver_path), "%s", base_ver_path);
            else
                snprintf(ver_path, sizeof(ver_path), "%s.%d", base_ver_path, idx);

            if (stat(ver_path, &tmp) != 0)
            {
                break;
            }
            idx++;
        }

        if (rename(dst, ver_path) != 0)
        {
            fprintf(stderr, "backup: rename(%s -> %s) failed: %s\n", dst, ver_path, strerror(errno));
            return -1;
        }

        char dirbuf[PATH_MAX];
        get_dirname(dst, dirbuf, sizeof(dirbuf));
        if (mkdir_p(dirbuf, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", dirbuf);
            return -1;
        }

        if (copy_file(path, dst, st) != 0)
            return -1;

        pthread_mutex_lock(&ctx->lock);
        ctx->cnt_updated++;
        strlist_push(&ctx->updated_files, rel);
        pthread_mutex_unlock(&ctx->lock);
    }
    else
    {
        pthread_mutex_lock(&ctx->lock);
        ctx->cnt_skipped++;
        strlist_push(&ctx->skipped_files, rel);
        pthread_mutex_unlock(&ctx->lock);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int compress = 0;
    int check_only = 0;
    const char *src_arg = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--compress") == 0)
        {
            compress = 1;
        }
        else if (strcmp(argv[i], "--check") == 0)
        {
            check_only = 1;
        }
        else
        {
            if (src_arg)
            {
                fprintf(stderr, "Usage: backup [--compress] [--check] DIR\n");
                return 1;
            }
            src_arg = argv[i];
        }
    }

    if (!src_arg)
    {
        fprintf(stderr, "Usage: backup [--compress] [--check] DIR\n");
        return 1;
    }

    if (compress && check_only)
    {
        fprintf(stderr, "backup: --compress and --check together don't make sense\n");
        return 1;
    }

    char src_real[PATH_MAX];
    if (!realpath(src_arg, src_real))
    {
        fprintf(stderr, "backup: cannot resolve %s: %s\n", src_arg, strerror(errno));
        return 1;
    }

    struct stat st_src;
    if (stat(src_real, &st_src) != 0 || !S_ISDIR(st_src.st_mode))
    {
        fprintf(stderr, "backup: %s is not a directory\n", src_real);
        return 1;
    }

    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "backup: HOME is not set\n");
        return 1;
    }

    char timebuf[32];
    get_current_datetime(timebuf, sizeof(timebuf));

    char date_only[11];
    memcpy(date_only, timebuf, 10);
    date_only[10] = '\0';

    char src_copy[PATH_MAX];
    snprintf(src_copy, sizeof(src_copy), "%s", src_real);
    size_t l = strlen(src_copy);
    while (l > 1 && src_copy[l - 1] == '/')
    {
        src_copy[l - 1] = '\0';
        l--;
    }
    const char *src_base = get_basename(src_copy);

    char backups_root[PATH_MAX];
    snprintf(backups_root, sizeof(backups_root), "%s/Backups", home);
    if (!check_only)
    {
        if (mkdir_p(backups_root, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", backups_root);
            return 1;
        }
    }

    char backup_root[PATH_MAX];
    snprintf(backup_root, sizeof(backup_root), "%s/%s-%s", backups_root, src_base, date_only);

    if (!check_only)
    {
        if (mkdir_p(backup_root, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", backup_root);
            return 1;
        }

        char versions_dir[PATH_MAX];
        snprintf(versions_dir, sizeof(versions_dir), "%s/.versions", backup_root);
        if (mkdir_p(versions_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", versions_dir);
            return 1;
        }
    }
    if (!check_only)
    {
        char spath[PATH_MAX];
        snprintf(spath, sizeof(spath), "%s/.source_path", backup_root);

        char line[PATH_MAX + 4];
        snprintf(line, sizeof(line), "SOURCE=%s\n", src_real);

        if (write_text_file_atomic(spath, line) != 0)
        {
            fprintf(stderr, "backup: warning: failed to write .source_path\n");
        }
    }

    backup_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.src_root, sizeof(ctx.src_root), "%s", src_real);
    ctx.src_root_len = strlen(ctx.src_root);
    snprintf(ctx.backup_root, sizeof(ctx.backup_root), "%s", backup_root);
    snprintf(ctx.datetime, sizeof(ctx.datetime), "%s", timebuf);
    ctx.check_only = check_only;
    ctx.compress = compress;
    strlist_init(&ctx.new_files);
    strlist_init(&ctx.updated_files);
    strlist_init(&ctx.skipped_files);
    pthread_mutex_init(&ctx.lock, NULL);

    crawler_config_t conf = { .max_threads = 4, .follow_symlinks = 0, .max_depth = UINT_MAX, .file_types = CRAWL_F_REG, .crawl_through = 1 };

    if (crawl_directory(ctx.src_root, &conf, process_file, &ctx) != 0)
    {
        fprintf(stderr, "backup: crawl_directory failed\n");
    }

    if (check_only)
    {
        printf("Changed:");
        for (size_t i = 0; i < ctx.updated_files.len; i++)
            printf("%s%s", (i == 0 ? " " : ","), ctx.updated_files.items[i]);
        printf("\n");

        printf("New:");
        for (size_t i = 0; i < ctx.new_files.len; i++)
            printf("%s%s", (i == 0 ? " " : ","), ctx.new_files.items[i]);
        printf("\n");

        printf("Unchanged:");
        for (size_t i = 0; i < ctx.skipped_files.len; i++)
            printf("%s%s", (i == 0 ? " " : ","), ctx.skipped_files.items[i]);
        printf("\n");

        pthread_mutex_destroy(&ctx.lock);
        strlist_free(&ctx.new_files);
        strlist_free(&ctx.updated_files);
        strlist_free(&ctx.skipped_files);
        return 0;
    }

    if (compress)
    {
        char versions_dir[PATH_MAX];
        snprintf(versions_dir, sizeof(versions_dir), "%s/.versions", ctx.backup_root);
        struct stat stv;
        if (stat(versions_dir, &stv) == 0 && S_ISDIR(stv.st_mode))
        {
            char cmd[PATH_MAX * 3];
            snprintf(cmd, sizeof(cmd), "cd '%s' && tar -czf versions.tar.gz .versions", ctx.backup_root);
            int rc = system(cmd);
            if (rc == -1)
                fprintf(stderr, "backup: failed to run tar\n");
        }
    }

    printf("=== Backup | %s ===\n", date_only);

    printf("New:");
    for (size_t i = 0; i < ctx.new_files.len; i++)
        printf("%s%s", (i == 0 ? " " : ", "), ctx.new_files.items[i]);
    printf("\n");

    printf("Updated:");
    for (size_t i = 0; i < ctx.updated_files.len; i++)
        printf("%s%s", (i == 0 ? " " : ", "), ctx.updated_files.items[i]);
    printf("\n");

    printf("Skipped:");
    for (size_t i = 0; i < ctx.skipped_files.len; i++)
        printf("%s%s", (i == 0 ? " " : ", "), ctx.skipped_files.items[i]);
    printf("\n");

    printf("---------------------------\n");
    printf("Summary: added %zu, updated %zu, skipped %zu\n", ctx.cnt_new, ctx.cnt_updated, ctx.cnt_skipped);
    printf("===========================\n");

    pthread_mutex_destroy(&ctx.lock);
    strlist_free(&ctx.new_files);
    strlist_free(&ctx.updated_files);
    strlist_free(&ctx.skipped_files);

    return 0;
}
