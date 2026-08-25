#define _XOPEN_SOURCE 700

#include "utils/crawler.h"
#include "utils/queue.h"

#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

typedef enum
{
    CALLBACK_CHEAP,
    CALLBACK_STAT,
    CALLBACK_HEAVY
} callback_mode_t;

typedef struct
{
    atomic_ullong entries;
    atomic_ullong bytes;
    atomic_ullong checksum;
    callback_mode_t mode;
} bench_state_t;

typedef struct
{
    const char *model;
    const char *tree;
    const char *callback;
    const char *root;
    unsigned threads;
    unsigned max_depth;
} bench_args_t;

typedef struct
{
    char *path;
    int depth;
    struct stat st;
} bench_task_t;

typedef struct
{
    queue_t *dir_q;
    queue_t *proc_q;
    const crawler_config_t *conf;
    bench_state_t *state;
    pthread_mutex_t mt;
    int active_dirs;
    int stopping;
    int result;
} model_a_ctx_t;

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t process_cpu_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static unsigned long hash_path(const char *path)
{
    unsigned long h = 1469598103934665603ull;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p)
    {
        h ^= *p;
        h *= 1099511628211ull;
    }
    return h;
}

static int bench_callback(const char *path, const struct stat *st, void *user_data)
{
    bench_state_t *state = (bench_state_t *)user_data;
    atomic_fetch_add_explicit(&state->entries, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&state->bytes, (unsigned long long)st->st_size, memory_order_relaxed);

    if (state->mode == CALLBACK_STAT)
    {
        struct stat tmp;
        if (lstat(path, &tmp) != 0)
            return CRAWL_PROC_ERROR;
        atomic_fetch_add_explicit(&state->bytes, (unsigned long long)tmp.st_size, memory_order_relaxed);
    }
    else if (state->mode == CALLBACK_HEAVY)
    {
        unsigned long h = 0;
        for (unsigned i = 0; i < 1000; ++i)
            h ^= hash_path(path) + i;
        atomic_fetch_xor_explicit(&state->checksum, (unsigned long long)h, memory_order_relaxed);
    }

    return CRAWL_PROC_CONTINUE;
}

static char *join_path_alloc(const char *dir, const char *name)
{
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    char *out = malloc(dir_len + name_len + 2);
    if (!out)
        return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, name, name_len + 1);
    return out;
}

static bench_task_t *make_task(char *path, int depth, const struct stat *st)
{
    bench_task_t *task = malloc(sizeof(bench_task_t));
    if (!task)
        return NULL;
    task->path = path;
    task->depth = depth;
    if (st)
        task->st = *st;
    else
        memset(&task->st, 0, sizeof(task->st));
    return task;
}

static void free_bench_task(bench_task_t *task)
{
    if (!task)
        return;
    free(task->path);
    free(task);
}

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

static int model_a_is_stopping(model_a_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->mt);
    int stopping = ctx->stopping;
    pthread_mutex_unlock(&ctx->mt);
    return stopping;
}

static void model_a_request_stop(model_a_ctx_t *ctx, int result)
{
    pthread_mutex_lock(&ctx->mt);
    ctx->stopping = 1;
    if (result != 0)
        ctx->result = result;
    pthread_mutex_unlock(&ctx->mt);

    queue_close(ctx->dir_q);
    queue_close(ctx->proc_q);
}

static void model_a_finish_dir(model_a_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->mt);
    ctx->active_dirs--;
    int should_close = ctx->active_dirs == 0;
    pthread_mutex_unlock(&ctx->mt);

    if (should_close)
        queue_close(ctx->dir_q);
}

static int model_a_add_dir(model_a_ctx_t *ctx, char *path, int depth, const struct stat *st)
{
    bench_task_t *task = make_task(path, depth, st);
    if (!task)
        return -1;

    pthread_mutex_lock(&ctx->mt);
    ctx->active_dirs++;
    pthread_mutex_unlock(&ctx->mt);

    queue_rc_t rc = queue_push(ctx->dir_q, task);
    if (rc != QUEUE_OK)
    {
        pthread_mutex_lock(&ctx->mt);
        ctx->active_dirs--;
        pthread_mutex_unlock(&ctx->mt);
        free_bench_task(task);
        return -1;
    }

    return 0;
}

static int model_a_add_proc(model_a_ctx_t *ctx, char *path, int depth, const struct stat *st)
{
    bench_task_t *task = make_task(path, depth, st);
    if (!task)
        return -1;

    queue_rc_t rc = queue_push(ctx->proc_q, task);
    if (rc != QUEUE_OK)
    {
        free_bench_task(task);
        return -1;
    }

    return 0;
}

static void *model_a_traversal_worker(void *arg)
{
    model_a_ctx_t *ctx = (model_a_ctx_t *)arg;

    for (;;)
    {
        bench_task_t *task = NULL;
        queue_rc_t rc = queue_pop(ctx->dir_q, (void **)&task);
        if (rc == QUEUE_CLOSED || rc == QUEUE_CANCELLED)
            break;
        if (rc != QUEUE_OK || !task)
        {
            model_a_request_stop(ctx, -1);
            return (void *)1;
        }

        if (model_a_is_stopping(ctx))
        {
            free_bench_task(task);
            model_a_finish_dir(ctx);
            continue;
        }

        DIR *dir = opendir(task->path);
        if (!dir)
        {
            free_bench_task(task);
            model_a_finish_dir(ctx);
            continue;
        }

        struct dirent *ent;
        while (!model_a_is_stopping(ctx) && (ent = readdir(dir)) != NULL)
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            char *path = join_path_alloc(task->path, ent->d_name);
            if (!path)
            {
                model_a_request_stop(ctx, -1);
                break;
            }

            struct stat st;
            if (ctx->conf->follow_symlinks ? stat(path, &st) != 0 : lstat(path, &st) != 0)
            {
                free(path);
                continue;
            }

            mode_t type = st.st_mode & S_IFMT;
            uint32_t mask = mask_for_type(type);
            int is_dir = type == S_IFDIR;
            int path_owned = 1;

            if (mask && (ctx->conf->file_types & mask))
            {
                char *proc_path = path;
                if (is_dir && ctx->conf->crawl_through && task->depth < (int)ctx->conf->max_depth)
                {
                    proc_path = strdup(path);
                    if (!proc_path)
                    {
                        free(path);
                        model_a_request_stop(ctx, -1);
                        break;
                    }
                }

                if (model_a_add_proc(ctx, proc_path, task->depth + 1, &st) != 0)
                {
                    if (proc_path != path)
                        free(path);
                    model_a_request_stop(ctx, -1);
                    break;
                }

                if (proc_path == path)
                    path_owned = 0;
            }

            if (is_dir && ctx->conf->crawl_through && task->depth < (int)ctx->conf->max_depth)
            {
                if (model_a_add_dir(ctx, path, task->depth + 1, &st) != 0)
                {
                    if (path_owned)
                        free(path);
                    model_a_request_stop(ctx, -1);
                    break;
                }
                path_owned = 0;
            }

            if (path_owned)
                free(path);
        }

        closedir(dir);
        free_bench_task(task);
        model_a_finish_dir(ctx);
    }

    return NULL;
}

static void *model_a_processing_worker(void *arg)
{
    model_a_ctx_t *ctx = (model_a_ctx_t *)arg;

    for (;;)
    {
        bench_task_t *task = NULL;
        queue_rc_t rc = queue_pop(ctx->proc_q, (void **)&task);
        if (rc == QUEUE_CLOSED || rc == QUEUE_CANCELLED)
            break;
        if (rc != QUEUE_OK || !task)
        {
            model_a_request_stop(ctx, -1);
            return (void *)1;
        }

        if (!model_a_is_stopping(ctx))
        {
            int cb_rc = bench_callback(task->path, &task->st, ctx->state);
            if (cb_rc == CRAWL_PROC_ERROR)
                model_a_request_stop(ctx, -1);
            else if (cb_rc == CRAWL_PROC_STOP)
                model_a_request_stop(ctx, 0);
        }

        free_bench_task(task);
    }

    return NULL;
}

static int run_model_a(const bench_args_t *args, bench_state_t *state)
{
    unsigned traversal_threads = args->threads == 1 ? 1 : args->threads / 2;
    unsigned processing_threads = args->threads - traversal_threads;
    if (processing_threads == 0)
        processing_threads = 1;

    queue_t *dir_q = make_queue(512);
    queue_t *proc_q = make_queue(1024);
    pthread_t *traversal = calloc(traversal_threads, sizeof(pthread_t));
    pthread_t *processing = calloc(processing_threads, sizeof(pthread_t));
    if (!dir_q || !proc_q || !traversal || !processing)
    {
        free_queue(dir_q);
        free_queue(proc_q);
        free(traversal);
        free(processing);
        return -1;
    }

    crawler_config_t conf = {
        .max_threads = args->threads,
        .max_depth = args->max_depth,
        .follow_symlinks = 0,
        .file_types = CRAWL_ALL,
        .crawl_through = 1,
    };

    model_a_ctx_t ctx = {
        .dir_q = dir_q,
        .proc_q = proc_q,
        .conf = &conf,
        .state = state,
        .active_dirs = 1,
        .stopping = 0,
        .result = 0,
    };
    pthread_mutex_init(&ctx.mt, NULL);

    char *root_path = strdup(args->root);
    bench_task_t *root_task = root_path ? make_task(root_path, 0, NULL) : NULL;
    if (!root_task || queue_push(dir_q, root_task) != QUEUE_OK)
    {
        free_bench_task(root_task);
        pthread_mutex_destroy(&ctx.mt);
        free_queue(dir_q);
        free_queue(proc_q);
        free(traversal);
        free(processing);
        return -1;
    }

    int rc = 0;
    for (unsigned i = 0; i < processing_threads; ++i)
    {
        if (pthread_create(&processing[i], NULL, model_a_processing_worker, &ctx) != 0)
        {
            model_a_request_stop(&ctx, -1);
            rc = -1;
            processing_threads = i;
            break;
        }
    }

    for (unsigned i = 0; rc == 0 && i < traversal_threads; ++i)
    {
        if (pthread_create(&traversal[i], NULL, model_a_traversal_worker, &ctx) != 0)
        {
            model_a_request_stop(&ctx, -1);
            rc = -1;
            traversal_threads = i;
            break;
        }
    }

    for (unsigned i = 0; i < traversal_threads; ++i)
        pthread_join(traversal[i], NULL);

    queue_close(proc_q);

    for (unsigned i = 0; i < processing_threads; ++i)
        pthread_join(processing[i], NULL);

    pthread_mutex_lock(&ctx.mt);
    if (ctx.result != 0)
        rc = ctx.result;
    pthread_mutex_unlock(&ctx.mt);

    pthread_mutex_destroy(&ctx.mt);
    free_queue(dir_q);
    free_queue(proc_q);
    free(traversal);
    free(processing);
    return rc;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --root DIR [--model A|B] [--tree NAME] [--callback cheap|stat|heavy] [--threads N] [--max-depth N]\n",
            prog);
}

static int parse_uint(const char *s, unsigned *out)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (!s[0] || !end || *end != '\0' || v == 0 || v > 1000000ul)
        return -1;
    *out = (unsigned)v;
    return 0;
}

static int parse_args(int argc, char **argv, bench_args_t *args)
{
    args->model = "B";
    args->tree = "unknown";
    args->callback = "cheap";
    args->root = NULL;
    args->threads = 1;
    args->max_depth = 1000000;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc)
            args->root = argv[++i];
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            args->model = argv[++i];
        else if (strcmp(argv[i], "--tree") == 0 && i + 1 < argc)
            args->tree = argv[++i];
        else if (strcmp(argv[i], "--callback") == 0 && i + 1 < argc)
            args->callback = argv[++i];
        else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc)
        {
            if (parse_uint(argv[++i], &args->threads) != 0)
                return -1;
        }
        else if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc)
        {
            if (parse_uint(argv[++i], &args->max_depth) != 0)
                return -1;
        }
        else
            return -1;
    }

    if (!args->root || (strcmp(args->model, "A") != 0 && strcmp(args->model, "B") != 0))
        return -1;
    if (strcmp(args->model, "A") == 0 && args->threads < 2)
        return -1;
    if (strcmp(args->callback, "cheap") != 0 && strcmp(args->callback, "stat") != 0 && strcmp(args->callback, "heavy") != 0)
        return -1;
    return 0;
}

static callback_mode_t callback_mode_from_name(const char *name)
{
    if (strcmp(name, "stat") == 0)
        return CALLBACK_STAT;
    if (strcmp(name, "heavy") == 0)
        return CALLBACK_HEAVY;
    return CALLBACK_CHEAP;
}

int main(int argc, char **argv)
{
    bench_args_t args;
    if (parse_args(argc, argv, &args) != 0)
    {
        usage(argv[0]);
        return 1;
    }

    bench_state_t state;
    atomic_init(&state.entries, 0);
    atomic_init(&state.bytes, 0);
    atomic_init(&state.checksum, 0);
    state.mode = callback_mode_from_name(args.callback);

    struct rusage before_ru;
    struct rusage after_ru;
    getrusage(RUSAGE_SELF, &before_ru);
    uint64_t wall_start = monotonic_ns();
    uint64_t cpu_start = process_cpu_ns();

    int rc;
    if (strcmp(args.model, "A") == 0)
    {
        rc = run_model_a(&args, &state);
    }
    else
    {
        crawler_config_t conf = {
            .max_threads = args.threads,
            .max_depth = args.max_depth,
            .follow_symlinks = 0,
            .file_types = CRAWL_ALL,
            .crawl_through = 1,
        };
        rc = crawl_directory(args.root, &conf, bench_callback, &state);
    }

    uint64_t cpu_end = process_cpu_ns();
    uint64_t wall_end = monotonic_ns();
    getrusage(RUSAGE_SELF, &after_ru);

    double wall_ms = (double)(wall_end - wall_start) / 1000000.0;
    double cpu_ms = (double)(cpu_end - cpu_start) / 1000000.0;
    double wall_sec = wall_ms / 1000.0;
    unsigned long long entries = atomic_load_explicit(&state.entries, memory_order_relaxed);
    unsigned long long bytes = atomic_load_explicit(&state.bytes, memory_order_relaxed);
    unsigned long long checksum = atomic_load_explicit(&state.checksum, memory_order_relaxed);
    double throughput = wall_sec > 0.0 ? (double)entries / wall_sec : 0.0;
    double cpu_util = wall_ms > 0.0 ? cpu_ms / wall_ms : 0.0;
    long vol_cs = after_ru.ru_nvcsw - before_ru.ru_nvcsw;
    long invol_cs = after_ru.ru_nivcsw - before_ru.ru_nivcsw;

    printf("%s,%s,%s,%u,%llu,%.3f,%.3f,%.3f,%.3f,%ld,%ld,%llu,%llu,%d\n",
           args.model,
           args.tree,
           args.callback,
           args.threads,
           entries,
           wall_ms,
           cpu_ms,
           cpu_util,
           throughput,
           vol_cs,
           invol_cs,
           bytes,
           checksum,
           rc);

    return rc == 0 ? 0 : 1;
}
