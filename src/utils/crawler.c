#define _XOPEN_SOURCE 700

#include "utils/crawler.h"

#include "utils/common.h"
#include "utils/queue.h"
#include <sys/stat.h>

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    queue_t *q;
    const crawler_config_t *conf;
    file_processor_t func;
    void *user_data;

    pthread_mutex_t *state_mt;
    int *active_tasks;
    int *stopping;
    int *result;
} worker_ctx_t;

static uint32_t mask_for_type(mode_t type)
{
    switch (type)
    {
    case S_IFREG:
        return CRAWL_F_REG;
    case S_IFDIR:
        return CRAWL_F_DIR;
    case S_IFLNK:
        return CRAWL_F_LNK;
    case S_IFIFO:
        return CRAWL_F_FIFO;
    case S_IFSOCK:
        return CRAWL_F_SOCK;
    case S_IFCHR:
        return CRAWL_F_CHR;
    case S_IFBLK:
        return CRAWL_F_BLK;
    default:
        return 0;
    }
}

static void free_task(task_t *t)
{
    if (!t)
        return;
    free(t->path);
    free(t);
}

static int is_stopping(worker_ctx_t *ctx)
{
    pthread_mutex_lock(ctx->state_mt);
    int stopping = *ctx->stopping;
    pthread_mutex_unlock(ctx->state_mt);
    return stopping;
}

static void request_stop(worker_ctx_t *ctx, int result)
{
    pthread_mutex_lock(ctx->state_mt);
    *ctx->stopping = 1;
    if (result != 0)
        *ctx->result = result;
    pthread_mutex_unlock(ctx->state_mt);

    queue_close(ctx->q);
}

static void finish_task(worker_ctx_t *ctx)
{
    pthread_mutex_lock(ctx->state_mt);
    (*ctx->active_tasks)--;
    int should_close = (*ctx->active_tasks == 0);
    pthread_mutex_unlock(ctx->state_mt);

    if (should_close)
        queue_close(ctx->q);
}

static void *worker(void *arg)
{
    worker_ctx_t *ctx = (worker_ctx_t *)arg;

    for (;;)
    {
        task_t *t = NULL;
        queue_rc_t c = queue_pop(ctx->q, (void **)&t);

        if (c == QUEUE_CLOSED || c == QUEUE_CANCELLED)
            break;
        if (c != QUEUE_OK || t == NULL)
        {
            request_stop(ctx, -1);
            return (void *)1;
        }

        if (is_stopping(ctx))
        {
            free_task(t);
            finish_task(ctx);
            continue;
        }

        DIR *d = opendir(t->path);
        if (!d)
        {
            fprintf(stderr, "crawler: failed to open dir: %s\n", t->path);
            free_task(t);
            finish_task(ctx);
            continue;
        }

        struct dirent *ent;
        while (!is_stopping(ctx) && (ent = readdir(d)) != NULL)
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            char *new_path = join_path(t->path, ent->d_name);
            if (!new_path)
                continue;

            struct stat st;
            if (ctx->conf->follow_symlinks ? stat(new_path, &st) == -1 : lstat(new_path, &st) == -1)
            {
                free(new_path);
                continue;
            }

            mode_t type = st.st_mode & S_IFMT;
            uint32_t m = mask_for_type(type);

            int is_dir = (type == S_IFDIR);

            int rc = CRAWL_PROC_CONTINUE;
            if (m && (ctx->conf->file_types & m))
            {
                rc = ctx->func(new_path, &st, ctx->user_data);

                if (rc == CRAWL_PROC_ERROR || rc == CRAWL_PROC_STOP)
                {
                    free(new_path);
                    request_stop(ctx, rc == CRAWL_PROC_ERROR ? -1 : 0);
                    break;
                }
            }
            if ((ctx->conf->crawl_through) && is_dir && (t->depth < ctx->conf->max_depth) && !(rc == CRAWL_PROC_SKIP_SUBTREE))
            {
                task_t *nt = malloc(sizeof(task_t));
                if (!nt)
                {
                    free(new_path);
                    request_stop(ctx, -1);
                    continue;
                }

                nt->depth = t->depth + 1;
                nt->path = new_path;

                pthread_mutex_lock(ctx->state_mt);
                (*ctx->active_tasks)++;
                pthread_mutex_unlock(ctx->state_mt);

                if (queue_push(ctx->q, nt) != QUEUE_OK)
                {
                    pthread_mutex_lock(ctx->state_mt);
                    (*ctx->active_tasks)--;
                    int already_stopping = *ctx->stopping;
                    pthread_mutex_unlock(ctx->state_mt);

                    free_task(nt);
                    if (!already_stopping)
                        request_stop(ctx, -1);
                    break;
                }
                continue;
            }
            free(new_path);
        }

        closedir(d);
        free_task(t);
        finish_task(ctx);
    }

    return NULL;
}

int crawl_directory(const char *root_path, const crawler_config_t *config, file_processor_t processor, void *user_data)
{
    if (!root_path || !config || !processor || config->max_threads == 0)
        return -1;

    queue_t *q = make_queue(512);
    if (!q)
        return -1;

    int rc = crawl_directory_q(q, root_path, config, processor, user_data);
    free_queue(q);

    return rc;
}

int crawl_directory_q(queue_t *q, const char *root_path, const crawler_config_t *config, file_processor_t processor, void *user_data)
{
    if (!q || !root_path || !config || !processor || (config->max_threads == 0))
        return -1;

    pthread_t *workers = malloc(sizeof(pthread_t) * config->max_threads);
    if (!workers)
        return -1;

    worker_ctx_t *ctx = malloc(sizeof(worker_ctx_t));
    if (!ctx)
    {
        free(workers);
        return -1;
    }

    pthread_mutex_t state_mt;
    pthread_mutex_init(&state_mt, NULL);
    int active_tasks = 1; // root task
    int stopping = 0;
    int result = 0;

    task_t *root_task = malloc(sizeof(task_t));
    if (!root_task)
    {
        pthread_mutex_destroy(&state_mt);
        free(ctx);
        free(workers);
        return -1;
    }

    root_task->depth = 0;
    root_task->path = strdup(root_path);
    if (!root_task->path)
    {
        free(root_task);
        pthread_mutex_destroy(&state_mt);
        free(ctx);
        free(workers);
        return -1;
    }

    if (queue_push(q, root_task) != QUEUE_OK)
    {
        free(root_task->path);
        free(root_task);
        pthread_mutex_destroy(&state_mt);
        free(ctx);
        free(workers);
        return -1;
    }

    ctx->q = q;
    ctx->conf = config;
    ctx->func = processor;
    ctx->user_data = user_data;
    ctx->state_mt = &state_mt;
    ctx->active_tasks = &active_tasks;
    ctx->stopping = &stopping;
    ctx->result = &result;

    size_t created = 0;
    for (size_t i = 0; i < config->max_threads; i++)
    {
        if (pthread_create(&workers[i], NULL, worker, ctx) == 0)
            created++;
        else
        {
            fprintf(stderr, "failed to create thread %zu of %u\n", i, config->max_threads);
            break;    // if couldn't create one thread it low possible to create next
        }
    }

    for (size_t i = 0; i < created; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_destroy(&state_mt);
    free(ctx);
    free(workers);

    return result;
}
