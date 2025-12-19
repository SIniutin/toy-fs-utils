#ifndef UTILS_QUEUE_H
#define UTILS_QUEUE_H

#include <pthread.h>

/**
 * @brief Single linked node of the queue
 *
 * Queue stores raw pointer memory managment of the
 * value is the caller's responsibilitty
 */
typedef struct node
{
    void *val;
    struct node *next;
} node_t;

/**
 *   @brief Thread-safe queue of raw pointers
 */
typedef struct queue
{
    node_t *head;
    node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int stop;
} queue_t;

/**
 *  @brief Queue constructor
 *  @return Pointer on queue or NULL
 */
queue_t *make_queue();

int queue_push(queue_t *q, void *val);

int queue_pop(queue_t *q, void **out_val);

void queue_stop(queue_t *q);

/**
 * @brief Queue destroyer
 * @
 */
void free_queue(queue_t *q);

#endif
