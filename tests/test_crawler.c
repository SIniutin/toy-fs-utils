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

int main(void)
{
    if (test_count_table() != 0)
        return 1;
    if (test_error_propagates() != 0)
        return 1;

    return 0;
}
