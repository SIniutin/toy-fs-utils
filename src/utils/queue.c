#include "utils/queue.h"

#include <pthread.h>
#include <stdlib.h>

queue_t *make_queue()
{
    queue_t *q = malloc(sizeof(queue_t));
    if (!q)
        return NULL;

    q->tail = q->head = NULL;
    q->stop = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

int queue_push(queue_t *q, void *val)
{
    if (!q)
        return 1;

    pthread_mutex_lock(&q->mutex);

    if (q->stop)
    {
        pthread_mutex_unlock(&q->mutex);
        return 1;
    }

    node_t *n = malloc(sizeof(node_t));
    if (!n)
    {
        pthread_mutex_unlock(&q->mutex);
        return 1;
    }

    n->val = val;
    n->next = NULL;

    if (q->tail)
        q->tail->next = n;
    else
        q->head = n;

    q->tail = n;

    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

int queue_pop(queue_t *q, void **out_val)
{
    if (!q || !out_val)
        return 1;

    pthread_mutex_lock(&q->mutex);

    while (!q->head && !q->stop)
    {
        pthread_cond_wait(&q->cond, &q->mutex);
    }

    if (!q->head && q->stop)
    {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    node_t *h = q->head;
    *out_val = h->val;
    q->head = h->next;
    if (!q->head)
        q->tail = NULL;

    free(h);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void queue_stop(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    q->stop = 1;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}
void free_queue(queue_t *q)
{
    if (!q)
        return;

    pthread_mutex_lock(&q->mutex);
    node_t *cur = q->head;
    while (cur)
    {
        node_t *next = cur->next;
        free(cur->val);
        free(cur);
        cur = next;
    }
    pthread_mutex_unlock(&q->mutex);

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}
