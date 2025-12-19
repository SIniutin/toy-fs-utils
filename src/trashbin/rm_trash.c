#define _XOPEN_SOURCE 700

#include "utils/common.h"
#include "utils/crawler.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    char verbose;
    char confirm;
    FILE *log;
    char trash_dir[PATH_MAX];
    pthread_mutex_t log_lock;
    pthread_mutex_t confirm_lock;
} data_t;

static int ensure_trash_dir(char *buf, size_t buf_sz)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "rm_trash: HOME is not set\n");
        return -1;
    }

    if (snprintf(buf, buf_sz, "%s/.trash", home) >= (int)buf_sz)
    {
        fprintf(stderr, "rm_trash: trash path is too long\n");
        return -1;
    }

    struct stat st;
    if (stat(buf, &st) == 0)
    {
        if (!S_ISDIR(st.st_mode))
        {
            fprintf(stderr, "rm_trash: %s exists and is not a directory\n", buf);
            return -1;
        }
        return 0;
    }

    if (mkdir(buf, 0700) != 0)
    {
        perror("rm_trash: mkdir ~/.trash");
        return -1;
    }

    return 0;
}

static FILE *open_log(void)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        fprintf(stderr, "rm_trash: HOME is not set\n");
        return NULL;
    }

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/.trash.log", home);
    if (n < 0 || n >= (int)sizeof(path))
    {
        fprintf(stderr, "rm_trash: log path is too long\n");
        return NULL;
    }

    FILE *f = fopen(path, "a");
    if (!f)
    {
        perror("rm_trash fopen ~/.trash.log");
    }
    return f;
}

static int ask_confirm(const char *path, pthread_mutex_t *lock)
{
    pthread_mutex_lock(lock);
    printf("rm_trash: remove \"%s\"? [y/N] ", path);
    fflush(stdout);

    int c = getchar();
    int first = c;

    while (c != '\n' && c != EOF)
        c = getchar();

    pthread_mutex_unlock(lock);
    return (first == 'y' || first == 'Y');
}

int process_file(const char *path, const struct stat *st, void *user_data)
{
    if (!path || !st || !user_data)
        return -1;

    data_t *data = (data_t *)user_data;

    if (!S_ISREG(st->st_mode))
    {
        if (data->verbose)
            printf("rm_trash: \"%s\" is not a regular file, skipping\n", path);
        return 0;
    }

    if (data->confirm && !ask_confirm(path, &data->confirm_lock))
    {
        if (data->verbose)
            printf("rm_trash: skipped \"%s\" by user\n", path);
        return 0;
    }

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    char timebuf[32];
    get_current_datetime(timebuf, sizeof(timebuf));

    char link_name[PATH_MAX];
    if (snprintf(link_name, sizeof(link_name), "%s@%s@%lu", name, timebuf, (unsigned long)st->st_ino) >= (int)sizeof(link_name))
    {
        fprintf(stderr, "rm_trash: link name for \"%s\" is too long\n", path);
        return 0;
    }

    char dst_path[PATH_MAX];
    if (snprintf(dst_path, sizeof(dst_path), "%s/%s", data->trash_dir, link_name) >= (int)sizeof(dst_path))
    {
        fprintf(stderr, "rm_trash: trash path for \"%s\" is too long\n", path);
        return 0;
    }
    pthread_mutex_lock(&data->log_lock);
    if (data->log)
    {
        fprintf(data->log, "%s | %s | %lu | %ld | %s\n", path, link_name, (unsigned long)st->st_ino, (long)st->st_size, timebuf);
        fflush(data->log);
    }
    pthread_mutex_unlock(&data->log_lock);

    if (data->verbose)
        printf("rm_trash: link \"%s\" -> \"%s\"\n", dst_path, path);

    if (link(path, dst_path) != 0)
    {
        fprintf(stderr, "rm_trash: failed to link \"%s\" -> \"%s\": %s\n", dst_path, path, strerror(errno));
        return 0;
    }

    if (data->verbose)
        printf("rm_trash: unlink \"%s\"\n", path);

    if (unlink(path) != 0)
    {
        fprintf(stderr, "rm_trash: failed to unlink \"%s\": %s\n", path, strerror(errno));
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        puts("rm_trash: nothing to do");
        return 0;
    }
    crawler_config_t config = { .max_threads = 4, .max_depth = 100000, .follow_symlinks = 0, .file_types = CRAWL_F_REG, .crawl_through = 1 };
    data_t data = { .verbose = 0, .confirm = 0, .log = NULL, .trash_dir = { 0 } };
    int argi = 1;
    for (; argi < argc && argv[argi][0] == '-'; ++argi)
    {
        if (strcmp(argv[argi], "-v") == 0)
            data.verbose = 1;
        else if (strcmp(argv[argi], "-p") == 0)
        {
            data.confirm = 1;
            if (pthread_mutex_init(&data.confirm_lock, NULL) != 0)
            {
                fprintf(stderr, "rm_trash: mutex initialization failed\n");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "rm_trash: unknown option \"%s\"\n", argv[argi]);
            return 1;
        }
    }

    if (argi >= argc)
    {
        puts("rm_trash: nothing to do");
        return 0;
    }

    if (ensure_trash_dir(data.trash_dir, sizeof(data.trash_dir)) != 0)
        return 1;

    data.log = open_log();
    if (!data.log)
        return 1;
    if (pthread_mutex_init(&data.log_lock, NULL) != 0)
    {
        fprintf(stderr, "rm_trash: mutex initialization failed\n");
        return 1;
    }

    for (int i = argi; i < argc; ++i)
    {
        const char *p = argv[i];
        struct stat st;
        if (lstat(p, &st) != 0)
        {
            fprintf(stderr, "rm_trash: couldn't stat \"%s\": %s\n", p, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (data.verbose)
                fprintf(stderr, "rm_trash: crawling directory \"%s\"\n", p);
            crawl_directory(p, &config, process_file, &data);
        }
        else
            process_file(p, &st, &data);
    }

    fclose(data.log);
    pthread_mutex_destroy(&data.log_lock);
    if (data.confirm)
    {
        pthread_mutex_destroy(&data.confirm_lock);
    }
    return 0;
}
