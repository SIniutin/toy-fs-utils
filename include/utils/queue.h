#ifndef UTILS_QUEUE_H
#define UTILS_QUEUE_H

#include <pthread.h>

typedef enum {
    QUEUE_OK = 0,
    QUEUE_CLOSED = 1,
    QUEUE_CANCELLED = 2,
    QUEUE_ERROR = -1
} queue_rc_t;

/**
 *   @brief Thread-safe blocking queue with bounded ring buffer
 *   @note Queue owns the ring buffer, not the values stored in it.
 */
typedef struct
{
    void **items;

    unsigned head;
    unsigned tail;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    unsigned size; //TODO is unsigned best here?
    unsigned cap;

    int closed;
    int cancelled;
} queue_t;

/**
 *  @brief Queue constructor,
 *  cap must be > 0
 *  @return Pointer on queue or NULL with error set in errno
 */
queue_t *make_queue(unsigned cap);

/**
 *  Blocking if full
 */
queue_rc_t queue_push(queue_t *q, void *val);

/**
 *  Blocking if empty
 */
queue_rc_t queue_pop(queue_t *q, void **out_val);

/**
 * No more pushes; consumers may drain queued values.
 */
void queue_close(queue_t *q);

/**
 * Wake up and stop as soon as possible.
 */
void queue_cancel(queue_t *q);

/**
 * @brief Queue destroyer
 */
void free_queue(queue_t *q);

#endif
