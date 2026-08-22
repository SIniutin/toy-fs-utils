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

#define NUM_THREADS 4

#define RM_TRASH_OK 0
#define RM_TRASH_USAGE 1
#define RM_TRASH_IO 2
#define RM_TRASH_INTERNAL 3

typedef struct
{
    char verbose;
    char confirm;
    FILE *log;
    char trash_dir[PATH_MAX];
    pthread_mutex_t log_lock;
    pthread_mutex_t confirm_lock;
} data_t;

static void print_help(void)
{
    puts("Usage: rm_trash [-h] [-v] [-i] [-r] FILE...");
    puts("  -h  show this help");
    puts("  -v  print trashed files");
    puts("  -i  prompt before each file");
    puts("  -r  recursively process regular files inside directories");
}

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
    if (snprintf(path, sizeof(path), "%s/.trash.log", home) >= (int)sizeof(path))
    {
        fprintf(stderr, "rm_trash: log path is too long\n");
        return NULL;
    }

    FILE *f = fopen(path, "a");
    if (!f)
        perror("rm_trash: fopen ~/.trash.log");
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

    if (data->verbose)
        printf("rm_trash: link \"%s\" -> \"%s\"\n", dst_path, path);

    if (link(path, dst_path) != 0)
    {
        fprintf(stderr, "rm_trash: failed to link \"%s\" -> \"%s\": %s\n", dst_path, path, strerror(errno));
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
        puts("rm_trash: Usage <files-to-trash> of -h for more info");
        return RM_TRASH_OK;
    }
    crawler_config_t config = {.max_threads = NUM_THREADS, .max_depth = UINT_MAX, .follow_symlinks = 0, .file_types = CRAWL_F_REG, .crawl_through = 0};
    data_t data = {.verbose = 0, .confirm = 0, .log = NULL, .trash_dir = {0}};
    int argi = 1;

    for (; argi < argc && argv[argi][0] == '-'; ++argi)
    {
        if (strcmp(argv[argi], "-v") == 0)
            data.verbose = 1;
        else if (strcmp(argv[argi], "-i") == 0)
        {
            data.confirm = 1;
        }
        else if (strcmp(argv[argi], "-h") == 0)
        {
            print_help();
            return RM_TRASH_OK;
        }
        else if (strcmp(argv[argi], "-r") == 0)
        {
            config.crawl_through = 1;
        }
        else
        {
            fprintf(stderr, "rm_trash: unknown option \"%s\"\n", argv[argi]);
            return RM_TRASH_USAGE;
        }
    }

    if (argi >= argc)
    {
        puts("rm_trash: nothing to do");
        return RM_TRASH_OK;
    }

    if (ensure_trash_dir(data.trash_dir, sizeof(data.trash_dir)) != 0)
        return RM_TRASH_USAGE;

    data.log = open_log();
    if (!data.log)
        return RM_TRASH_IO;

    if (pthread_mutex_init(&data.log_lock, NULL) != 0)
    {
        fprintf(stderr, "rm_trash: log mutex initialization failed\n");
        fclose(data.log);
        return RM_TRASH_INTERNAL;
    }

    if (data.confirm && pthread_mutex_init(&data.confirm_lock, NULL) != 0)
    {
        fprintf(stderr, "rm_trash: confirm mutex initialization failed\n");
        pthread_mutex_destroy(&data.log_lock);
        fclose(data.log);
        return RM_TRASH_INTERNAL;
    }

    int exit_status = RM_TRASH_OK;
    for (int i = argi; i < argc; ++i)
    {
        const char *p = argv[i];
        struct stat st;
        if (lstat(p, &st) != 0)
        {
            fprintf(stderr, "rm_trash: couldn't stat \"%s\": %s\n", p, strerror(errno));
            exit_status = RM_TRASH_IO;
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (!config.crawl_through)
            {
                fprintf(stderr, "rm_trash: \"%s\" is a directory; use -r\n", p);
                exit_status = RM_TRASH_USAGE;
                continue;
            }

            if (data.verbose)
                fprintf(stdout, "rm_trash: crawling directory \"%s\"\n", p);

            if (crawl_directory(p, &config, process_file, &data) != 0)
                exit_status = RM_TRASH_INTERNAL;
        }
        else
        {
            if (process_file(p, &st, &data) != 0)
                exit_status = RM_TRASH_IO;
        }
    }

    if (data.confirm)
        pthread_mutex_destroy(&data.confirm_lock);
    pthread_mutex_destroy(&data.log_lock);
    fclose(data.log);

    return exit_status;
}
