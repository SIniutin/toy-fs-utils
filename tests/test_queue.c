#include "utils/queue.h"

#include <stddef.h>
#include <pthread.h>
#include <stdio.h>
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
    const char *name;
    unsigned cap;
    int values[8];
    size_t nvalues;
} fifo_case_t;

static int run_fifo_case(const fifo_case_t *tc)
{
    queue_t *q = make_queue(tc->cap);
    ASSERT_TRUE(q != NULL);

    for (size_t i = 0; i < tc->nvalues; i++)
        ASSERT_TRUE(queue_push(q, (void *)&tc->values[i]) == QUEUE_OK);

    for (size_t i = 0; i < tc->nvalues; i++)
    {
        void *out = NULL;
        ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
        ASSERT_TRUE(out == &tc->values[i]);
    }

    free_queue(q);
    return 0;
}

static int test_fifo_table(void)
{
    fifo_case_t cases[] = {
        {.name = "single item", .cap = 1, .values = {1}, .nvalues = 1},
        {.name = "multiple items", .cap = 4, .values = {1, 2, 3}, .nvalues = 3},
        {.name = "full queue", .cap = 3, .values = {10, 20, 30}, .nvalues = 3},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_fifo_case(&cases[i]) != 0)
        {
            fprintf(stderr, "failed case: %s\n", cases[i].name);
            return 1;
        }
    }

    return 0;
}

static int test_wraparound(void)
{
    queue_t *q = make_queue(3);
    ASSERT_TRUE(q != NULL);

    int values[] = {1, 2, 3, 4, 5};
    void *out = NULL;

    ASSERT_TRUE(queue_push(q, &values[0]) == QUEUE_OK);
    ASSERT_TRUE(queue_push(q, &values[1]) == QUEUE_OK);
    ASSERT_TRUE(queue_push(q, &values[2]) == QUEUE_OK);
    ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
    ASSERT_TRUE(out == &values[0]);
    ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
    ASSERT_TRUE(out == &values[1]);

    ASSERT_TRUE(queue_push(q, &values[3]) == QUEUE_OK);
    ASSERT_TRUE(queue_push(q, &values[4]) == QUEUE_OK);

    ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
    ASSERT_TRUE(out == &values[2]);
    ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
    ASSERT_TRUE(out == &values[3]);
    ASSERT_TRUE(queue_pop(q, &out) == QUEUE_OK);
    ASSERT_TRUE(out == &values[4]);

    free_queue(q);
    return 0;
}

typedef struct
{
    queue_t *q;
    int *value;
    queue_rc_t rc;
} push_thread_ctx_t;

static void *blocking_push(void *arg)
{
    push_thread_ctx_t *ctx = arg;
    ctx->rc = queue_push(ctx->q, ctx->value);
    return NULL;
}

static int test_blocking_push_cancel(void)
{
    queue_t *q = make_queue(1);
    ASSERT_TRUE(q != NULL);

    int a = 1;
    int b = 2;
    ASSERT_TRUE(queue_push(q, &a) == QUEUE_OK);

    push_thread_ctx_t ctx = {.q = q, .value = &b, .rc = QUEUE_OK};
    pthread_t th;
    ASSERT_TRUE(pthread_create(&th, NULL, blocking_push, &ctx) == 0);
    usleep(10000);
    queue_cancel(q);
    ASSERT_TRUE(pthread_join(th, NULL) == 0);
    ASSERT_TRUE(ctx.rc == QUEUE_CANCELLED);

    free_queue(q);
    return 0;
}

static int test_null_operations(void)
{
    int value = 1;
    void *out = NULL;

    ASSERT_TRUE(make_queue(0) == NULL);
    ASSERT_TRUE(queue_push(NULL, &value) == QUEUE_ERROR);
    ASSERT_TRUE(queue_pop(NULL, &out) == QUEUE_ERROR);
    queue_close(NULL);
    queue_cancel(NULL);
    free_queue(NULL);

    return 0;
}

typedef struct
{
    const char *name;
    queue_rc_t expected_push;
    queue_rc_t expected_first_pop;
    queue_rc_t expected_second_pop;
    int close_queue;
    int cancel_queue;
} lifecycle_case_t;

static int run_lifecycle_case(const lifecycle_case_t *tc)
{
    queue_t *q = make_queue(2);
    ASSERT_TRUE(q != NULL);
    int a = 1;
    int b = 2;
    void *out = NULL;

    ASSERT_TRUE(queue_push(q, &a) == QUEUE_OK);
    if (tc->close_queue)
        queue_close(q);
    if (tc->cancel_queue)
        queue_cancel(q);

    ASSERT_TRUE(queue_push(q, &b) == tc->expected_push);
    ASSERT_TRUE(queue_pop(q, &out) == tc->expected_first_pop);
    if (tc->expected_first_pop == QUEUE_OK)
        ASSERT_TRUE(out == &a);
    ASSERT_TRUE(queue_pop(q, &out) == tc->expected_second_pop);

    free_queue(q);
    return 0;
}

static int test_lifecycle_table(void)
{
    lifecycle_case_t cases[] = {
        {
            .name = "close drains existing values",
            .expected_push = QUEUE_CLOSED,
            .expected_first_pop = QUEUE_OK,
            .expected_second_pop = QUEUE_CLOSED,
            .close_queue = 1,
        },
        {
            .name = "cancel stops immediately",
            .expected_push = QUEUE_CANCELLED,
            .expected_first_pop = QUEUE_CANCELLED,
            .expected_second_pop = QUEUE_CANCELLED,
            .cancel_queue = 1,
        },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_lifecycle_case(&cases[i]) != 0)
        {
            fprintf(stderr, "failed case: %s\n", cases[i].name);
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    if (test_fifo_table() != 0)
        return 1;
    if (test_wraparound() != 0)
        return 1;
    if (test_blocking_push_cancel() != 0)
        return 1;
    if (test_null_operations() != 0)
        return 1;
    if (test_lifecycle_table() != 0)
        return 1;

    return 0;
}
