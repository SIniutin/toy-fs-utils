#define _XOPEN_SOURCE 700

#include "archive/file_ops.h"
#include "archive/paths.h"
#include "archive/snapshot.h"
#include "utils/common.h"
#include "utils/crawler.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static int process_file(const char *path, const struct stat *st, void *user_data)
{
    backup_ctx_t *ctx = user_data;

    if (!S_ISREG(st->st_mode))
        return 0;

    const char *rel = path + ctx->src_root_len;
    if (*rel == '/')
        rel++;

    char dst[PATH_MAX];
    if (join_path_into(dst, sizeof(dst), ctx->backup_root, rel) != 0)
    {
        fprintf(stderr, "backup: destination path is too long: %s/%s\n", ctx->backup_root, rel);
        return -1;
    }

    archive_file_status_t stt = archive_compare_with_dst(st, dst, "backup");

    if (ctx->check_only)
    {
        pthread_mutex_lock(&ctx->lock);
        switch (stt)
        {
        case ARCHIVE_FILE_NEW:
            strlist_push(&ctx->new_files, rel);
            break;
        case ARCHIVE_FILE_CHANGED:
            strlist_push(&ctx->updated_files, rel);
            break;
        case ARCHIVE_FILE_UNCHANGED:
            strlist_push(&ctx->skipped_files, rel);
            break;
        }
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }

    if (stt == ARCHIVE_FILE_NEW)
    {
        char dirbuf[PATH_MAX];
        archive_dirname(dst, dirbuf, sizeof(dirbuf));
        if (mkdir_p(dirbuf, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", dirbuf);
            return -1;
        }

        if (archive_copy_file(path, dst, st, "backup") != 0)
            return -1;

        pthread_mutex_lock(&ctx->lock);
        ctx->cnt_new++;
        strlist_push(&ctx->new_files, rel);
        pthread_mutex_unlock(&ctx->lock);
    }
    else if (stt == ARCHIVE_FILE_CHANGED)
    {
        char versions_dir[PATH_MAX];
        if (archive_versions_dir(versions_dir, sizeof(versions_dir), ctx->backup_root) != 0)
        {
            fprintf(stderr, "backup: versions path is too long: %s/.versions\n", ctx->backup_root);
            return -1;
        }
        if (mkdir_p(versions_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", versions_dir);
            return -1;
        }

        char ver_path[PATH_MAX];
        if (archive_version_path(ctx->backup_root, rel, ctx->datetime, ver_path, sizeof(ver_path)) != 0)
        {
            fprintf(stderr, "backup: version path is too long: %s/%s@%s\n", versions_dir, rel, ctx->datetime);
            return -1;
        }

        char ver_dir[PATH_MAX];
        archive_dirname(ver_path, ver_dir, sizeof(ver_dir));
        if (mkdir_p(ver_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", ver_dir);
            return -1;
        }

        if (rename(dst, ver_path) != 0)
        {
            fprintf(stderr, "backup: rename(%s -> %s) failed: %s\n", dst, ver_path, strerror(errno));
            return -1;
        }

        char dirbuf[PATH_MAX];
        archive_dirname(dst, dirbuf, sizeof(dirbuf));
        if (mkdir_p(dirbuf, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", dirbuf);
            return -1;
        }

        if (archive_copy_file(path, dst, st, "backup") != 0)
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
    if (snprintf_checked(src_copy, sizeof(src_copy), "%s", src_real) != 0)
    {
        fprintf(stderr, "backup: source path is too long\n");
        return 1;
    }
    size_t l = strlen(src_copy);
    while (l > 1 && src_copy[l - 1] == '/')
    {
        src_copy[l - 1] = '\0';
        l--;
    }
    const char *src_base = get_basename(src_copy);

    char backups_root[PATH_MAX];
    if (archive_backups_dir(backups_root, sizeof(backups_root)) != 0)
    {
        fprintf(stderr, "backup: Backups path is too long\n");
        return 1;
    }
    if (!check_only)
    {
        if (mkdir_p(backups_root, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", backups_root);
            return 1;
        }
    }

    char backup_root[PATH_MAX];
    if (snprintf_checked(backup_root, sizeof(backup_root), "%s/%s-%s", backups_root, src_base, date_only) != 0)
    {
        fprintf(stderr, "backup: backup path is too long: %s/%s-%s\n", backups_root, src_base, date_only);
        return 1;
    }

    if (!check_only)
    {
        if (mkdir_p(backup_root, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", backup_root);
            return 1;
        }

        char versions_dir[PATH_MAX];
        if (archive_versions_dir(versions_dir, sizeof(versions_dir), backup_root) != 0)
        {
            fprintf(stderr, "backup: versions path is too long: %s/.versions\n", backup_root);
            return 1;
        }
        if (mkdir_p(versions_dir, 0755) != 0)
        {
            fprintf(stderr, "backup: cannot create %s\n", versions_dir);
            return 1;
        }
    }
    if (!check_only)
    {
        char spath[PATH_MAX];
        if (archive_source_path_file(spath, sizeof(spath), backup_root) != 0)
        {
            fprintf(stderr, "backup: source path metadata path is too long: %s/.source_path\n", backup_root);
            return 1;
        }

        char line[PATH_MAX + 16];
        if (snprintf_checked(line, sizeof(line), "SOURCE=%s\n", src_real) != 0)
        {
            fprintf(stderr, "backup: source path metadata is too long\n");
            return 1;
        }

        if (archive_write_text_file_atomic(spath, line, "backup") != 0)
        {
            fprintf(stderr, "backup: warning: failed to write .source_path\n");
        }
    }

    backup_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strlist_init(&ctx.new_files);
    strlist_init(&ctx.updated_files);
    strlist_init(&ctx.skipped_files);
    if (pthread_mutex_init(&ctx.lock, NULL) != 0)
    {
        fprintf(stderr, "backup: mutex initialization failed\n");
        return 1;
    }

    if (snprintf_checked(ctx.src_root, sizeof(ctx.src_root), "%s", src_real) != 0 ||
        snprintf_checked(ctx.backup_root, sizeof(ctx.backup_root), "%s", backup_root) != 0 ||
        snprintf_checked(ctx.datetime, sizeof(ctx.datetime), "%s", timebuf) != 0)
    {
        fprintf(stderr, "backup: internal path buffer is too small\n");
        pthread_mutex_destroy(&ctx.lock);
        strlist_free(&ctx.new_files);
        strlist_free(&ctx.updated_files);
        strlist_free(&ctx.skipped_files);
        return 1;
    }
    ctx.src_root_len = strlen(ctx.src_root);
    ctx.check_only = check_only;
    ctx.compress = compress;

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
        if (archive_versions_dir(versions_dir, sizeof(versions_dir), ctx.backup_root) != 0)
        {
            fprintf(stderr, "backup: versions path is too long: %s/.versions\n", ctx.backup_root);
            return 1;
        }
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
