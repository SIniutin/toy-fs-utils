#define _XOPEN_SOURCE 700

#include "utils/crawler.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ASSERT_TRUE(expr)                                                                                              \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(expr))                                                                                                  \
        {                                                                                                             \
            fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                              \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

typedef struct
{
    unsigned count;
    pthread_mutex_t lock;
} count_ctx_t;

typedef struct
{
    unsigned regular;
    unsigned dirs;
    unsigned links;
    pthread_mutex_t lock;
} type_count_ctx_t;

typedef struct
{
    unsigned count;
    unsigned stop_after;
    pthread_mutex_t lock;
} stop_after_ctx_t;

static int count_file(const char *path, const struct stat *st, void *user_data)
{
    (void)path;
    if (!S_ISREG(st->st_mode))
        return CRAWL_PROC_CONTINUE;

    count_ctx_t *ctx = user_data;
    pthread_mutex_lock(&ctx->lock);
    ctx->count++;
    pthread_mutex_unlock(&ctx->lock);
    return CRAWL_PROC_CONTINUE;
}

static int fail_on_first_file(const char *path, const struct stat *st, void *user_data)
{
    (void)path;
    (void)st;
    (void)user_data;
    return CRAWL_PROC_ERROR;
}

static int stop_on_first_file(const char *path, const struct stat *st, void *user_data)
{
    (void)path;
    (void)st;
    (void)user_data;
    return CRAWL_PROC_STOP;
}

static int count_types(const char *path, const struct stat *st, void *user_data)
{
    (void)path;
    type_count_ctx_t *ctx = user_data;

    pthread_mutex_lock(&ctx->lock);
    if (S_ISREG(st->st_mode))
        ctx->regular++;
    else if (S_ISDIR(st->st_mode))
        ctx->dirs++;
    else if (S_ISLNK(st->st_mode))
        ctx->links++;
    pthread_mutex_unlock(&ctx->lock);

    return CRAWL_PROC_CONTINUE;
}

static int stop_after_n_files(const char *path, const struct stat *st, void *user_data)
{
    (void)path;

    if (!S_ISREG(st->st_mode))
        return CRAWL_PROC_CONTINUE;

    stop_after_ctx_t *ctx = user_data;
    pthread_mutex_lock(&ctx->lock);
    ctx->count++;
    int should_stop = ctx->count >= ctx->stop_after;
    pthread_mutex_unlock(&ctx->lock);

    return should_stop ? CRAWL_PROC_STOP : CRAWL_PROC_CONTINUE;
}

static int skip_dir_subtree(const char *path, const struct stat *st, void *user_data)
{
    count_ctx_t *ctx = user_data;

    if (S_ISREG(st->st_mode))
    {
        pthread_mutex_lock(&ctx->lock);
        ctx->count++;
        pthread_mutex_unlock(&ctx->lock);
        return CRAWL_PROC_CONTINUE;
    }

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (S_ISDIR(st->st_mode) && strcmp(base, "dir") == 0)
        return CRAWL_PROC_SKIP_SUBTREE;

    return CRAWL_PROC_CONTINUE;
}

static int write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    return fclose(f);
}

static int make_tree(char *template)
{
    char *root = mkdtemp(template);
    if (!root)
        return -1;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/dir", root);
    if (mkdir(path, 0700) != 0)
        return -1;
    snprintf(path, sizeof(path), "%s/dir/sub", root);
    if (mkdir(path, 0700) != 0)
        return -1;

    snprintf(path, sizeof(path), "%s/a.txt", root);
    if (write_file(path, "a") != 0)
        return -1;
    snprintf(path, sizeof(path), "%s/dir/b.log", root);
    if (write_file(path, "b") != 0)
        return -1;
    snprintf(path, sizeof(path), "%s/dir/sub/c.tmp", root);
    if (write_file(path, "c") != 0)
        return -1;

    return 0;
}

static int make_wide_tree(char *template, unsigned dirs, unsigned files_per_dir)
{
    char *root = mkdtemp(template);
    if (!root)
        return -1;

    char path[PATH_MAX];
    for (unsigned d = 0; d < dirs; d++)
    {
        snprintf(path, sizeof(path), "%s/dir_%u", root, d);
        if (mkdir(path, 0700) != 0)
            return -1;

        for (unsigned f = 0; f < files_per_dir; f++)
        {
            snprintf(path, sizeof(path), "%s/dir_%u/file_%u.txt", root, d, f);
            if (write_file(path, "x") != 0)
                return -1;
        }
    }

    return 0;
}

typedef struct
{
    const char *name;
    unsigned threads;
    unsigned max_depth;
    unsigned expected_files;
} count_case_t;

static int run_count_case(const count_case_t *tc)
{
    char root[] = "/tmp/toyfs-crawler.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = tc->threads,
        .max_depth = tc->max_depth,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, count_file, &ctx) == 0);
    ASSERT_TRUE(ctx.count == tc->expected_files);
    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_count_table(void)
{
    count_case_t cases[] = {
        {.name = "single thread recursive", .threads = 1, .max_depth = UINT_MAX, .expected_files = 3},
        {.name = "multi thread recursive", .threads = 4, .max_depth = UINT_MAX, .expected_files = 3},
        {.name = "max depth zero", .threads = 2, .max_depth = 0, .expected_files = 1},
        {.name = "max depth one", .threads = 2, .max_depth = 1, .expected_files = 2},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_count_case(&cases[i]) != 0)
        {
            fprintf(stderr, "failed case: %s\n", cases[i].name);
            return 1;
        }
    }

    return 0;
}

typedef struct
{
    const char *name;
    file_processor_t processor;
    int expect_success;
} result_case_t;

static int run_result_case(const result_case_t *tc)
{
    char root[] = "/tmp/toyfs-crawler.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    crawler_config_t conf = {
        .max_threads = 2,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG,
        .crawl_through = 1,
    };

    int rc = crawl_directory(root, &conf, tc->processor, NULL);
    ASSERT_TRUE(tc->expect_success ? rc == 0 : rc != 0);
    return 0;
}

static int test_error_propagates(void)
{
    result_case_t cases[] = {
        {.name = "callback error propagates", .processor = fail_on_first_file, .expect_success = 0},
        {.name = "callback stop returns success", .processor = stop_on_first_file, .expect_success = 1},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_result_case(&cases[i]) != 0)
        {
            fprintf(stderr, "failed case: %s\n", cases[i].name);
            return 1;
        }
    }

    return 0;
}

static int test_skip_subtree(void)
{
    char root[] = "/tmp/toyfs-crawler.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 2,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG | CRAWL_F_DIR,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, skip_dir_subtree, &ctx) == 0);
    ASSERT_TRUE(ctx.count == 1);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

typedef struct
{
    const char *name;
    uint32_t file_types;
    unsigned expected_regular;
    unsigned expected_dirs;
    unsigned expected_links;
} type_case_t;

static int run_type_case(const type_case_t *tc)
{
    char root[] = "/tmp/toyfs-crawler-types.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    char target[PATH_MAX];
    char link_path[PATH_MAX];
    snprintf(target, sizeof(target), "%s/a.txt", root);
    snprintf(link_path, sizeof(link_path), "%s/a.link", root);
    ASSERT_TRUE(symlink(target, link_path) == 0);

    type_count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 3,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = tc->file_types,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, count_types, &ctx) == 0);
    ASSERT_TRUE(ctx.regular == tc->expected_regular);
    ASSERT_TRUE(ctx.dirs == tc->expected_dirs);
    ASSERT_TRUE(ctx.links == tc->expected_links);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_file_type_filtering(void)
{
    type_case_t cases[] = {
        {.name = "regular only", .file_types = CRAWL_F_REG, .expected_regular = 3, .expected_dirs = 0, .expected_links = 0},
        {.name = "directories only", .file_types = CRAWL_F_DIR, .expected_regular = 0, .expected_dirs = 2, .expected_links = 0},
        {.name = "links only", .file_types = CRAWL_F_LNK, .expected_regular = 0, .expected_dirs = 0, .expected_links = 1},
        {.name = "regular directories and links", .file_types = CRAWL_F_REG | CRAWL_F_DIR | CRAWL_F_LNK, .expected_regular = 3, .expected_dirs = 2, .expected_links = 1},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_type_case(&cases[i]) != 0)
        {
            fprintf(stderr, "failed case: %s\n", cases[i].name);
            return 1;
        }
    }

    return 0;
}

static int test_follow_symlink_file(void)
{
    char root[] = "/tmp/toyfs-crawler-symlink.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    char target[PATH_MAX];
    char link_path[PATH_MAX];
    snprintf(target, sizeof(target), "%s/a.txt", root);
    snprintf(link_path, sizeof(link_path), "%s/a.link", root);
    ASSERT_TRUE(symlink(target, link_path) == 0);

    type_count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 2,
        .max_depth = UINT_MAX,
        .follow_symlinks = 1,
        .file_types = CRAWL_F_REG | CRAWL_F_LNK,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, count_types, &ctx) == 0);
    ASSERT_TRUE(ctx.regular == 4);
    ASSERT_TRUE(ctx.links == 0);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_crawl_through_disabled(void)
{
    char root[] = "/tmp/toyfs-crawler-through.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    type_count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 2,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG | CRAWL_F_DIR,
        .crawl_through = 0,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, count_types, &ctx) == 0);
    ASSERT_TRUE(ctx.regular == 1);
    ASSERT_TRUE(ctx.dirs == 1);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_wide_tree_multithreaded(void)
{
    enum
    {
        DIRS = 32,
        FILES_PER_DIR = 8,
    };

    char root[] = "/tmp/toyfs-crawler-wide.XXXXXX";
    ASSERT_TRUE(make_wide_tree(root, DIRS, FILES_PER_DIR) == 0);

    count_ctx_t ctx = {0};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 8,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, count_file, &ctx) == 0);
    ASSERT_TRUE(ctx.count == DIRS * FILES_PER_DIR);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_stop_multithreaded_finishes(void)
{
    char root[] = "/tmp/toyfs-crawler-stop.XXXXXX";
    ASSERT_TRUE(make_wide_tree(root, 16, 8) == 0);

    stop_after_ctx_t ctx = {.stop_after = 5};
    ASSERT_TRUE(pthread_mutex_init(&ctx.lock, NULL) == 0);

    crawler_config_t conf = {
        .max_threads = 6,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(root, &conf, stop_after_n_files, &ctx) == 0);
    ASSERT_TRUE(ctx.count >= ctx.stop_after);
    ASSERT_TRUE(ctx.count < 16 * 8);

    pthread_mutex_destroy(&ctx.lock);
    return 0;
}

static int test_invalid_arguments(void)
{
    char root[] = "/tmp/toyfs-crawler-invalid.XXXXXX";
    ASSERT_TRUE(make_tree(root) == 0);

    crawler_config_t conf = {
        .max_threads = 2,
        .max_depth = UINT_MAX,
        .follow_symlinks = 0,
        .file_types = CRAWL_F_REG,
        .crawl_through = 1,
    };

    ASSERT_TRUE(crawl_directory(NULL, &conf, count_file, NULL) != 0);
    ASSERT_TRUE(crawl_directory(root, NULL, count_file, NULL) != 0);
    ASSERT_TRUE(crawl_directory(root, &conf, NULL, NULL) != 0);

    conf.max_threads = 0;
    ASSERT_TRUE(crawl_directory(root, &conf, count_file, NULL) != 0);

    return 0;
}

int main(void)
{
    if (test_count_table() != 0)
        return 1;
    if (test_error_propagates() != 0)
        return 1;
    if (test_skip_subtree() != 0)
        return 1;
    if (test_file_type_filtering() != 0)
        return 1;
    if (test_follow_symlink_file() != 0)
        return 1;
    if (test_crawl_through_disabled() != 0)
        return 1;
    if (test_wide_tree_multithreaded() != 0)
        return 1;
    if (test_stop_multithreaded_finishes() != 0)
        return 1;
    if (test_invalid_arguments() != 0)
        return 1;

    return 0;
}
