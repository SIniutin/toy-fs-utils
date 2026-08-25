#include "utils/queue.h"

#include <errno.h>
#include <stdlib.h>

static void wake_all(queue_t *q)
{
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
}

queue_t *make_queue(unsigned cap)
{
    if (cap == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    queue_t *q = (queue_t *)malloc(sizeof(queue_t));
    if (!q)
        return NULL;

    q->cap = cap;
    q->size = 0;
    q->closed = 0;
    q->cancelled = 0;
    q->items = malloc(sizeof(void *) * cap);
    if (!q->items)
    {
        free(q);
        errno = ENOMEM;
        return NULL;
    }
    q->head = 0;
    q->tail = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0)
    {
        free(q->items);
        free(q);
        errno = ENOMEM;
        return NULL;
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0)
    {
        pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        errno = ENOMEM;
        return NULL;
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0)
    {
        pthread_cond_destroy(&q->not_full);
        pthread_mutex_destroy(&q->mutex);
        free(q->items);
        free(q);
        errno = ENOMEM;
        return NULL;
    }

    return q;
}

queue_rc_t queue_push(queue_t *q, void *val)
{
    if (!q)
        return QUEUE_ERROR;

    pthread_mutex_lock(&q->mutex);

    if (q->cancelled)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CANCELLED;
    }
    if (q->closed)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CLOSED;
    }

    while (!q->cancelled && !q->closed && q->size == q->cap)
        pthread_cond_wait(&q->not_full, &q->mutex);

    if (q->cancelled)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CANCELLED;
    }
    if (q->closed)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CLOSED;
    }

    q->items[q->tail] = val;
    q->tail = (q->tail + 1) % q->cap;
    q->size++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return QUEUE_OK;
}

queue_rc_t queue_pop(queue_t *q, void **out_val)
{
    if (!q || !out_val)
        return QUEUE_ERROR;

    pthread_mutex_lock(&q->mutex);

    while (!q->cancelled && q->size == 0 && !q->closed)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    if (q->cancelled)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CANCELLED;
    }

    if (q->size == 0 && q->closed)
    {
        pthread_mutex_unlock(&q->mutex);
        return QUEUE_CLOSED;
    }

    *out_val = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % q->cap;
    q->size--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return QUEUE_OK;
}

void queue_close(queue_t *q)
{
    if (!q)
        return;
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    wake_all(q);
    pthread_mutex_unlock(&q->mutex);
}

void queue_cancel(queue_t *q)
{
    if (!q)
        return;
    pthread_mutex_lock(&q->mutex);
    q->cancelled = 1;
    wake_all(q);
    pthread_mutex_unlock(&q->mutex);
}

void free_queue(queue_t *q)
{
    if (!q)
        return;

    pthread_mutex_lock(&q->mutex);
    if (q->size != 0)
    {
        pthread_mutex_unlock(&q->mutex);
        errno = EPERM;
        return;
    }

    q->head = 0;
    q->tail = 0;
    q->size = 0;
    q->closed = 1;
    q->cancelled = 1;
    wake_all(q);
    pthread_mutex_unlock(&q->mutex);

    free(q->items);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->mutex);
    free(q);
}
