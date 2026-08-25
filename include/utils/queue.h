#ifndef UTILS_QUEUE_H
#define UTILS_QUEUE_H

typedef enum {
    QUEUE_OK = 0,
    QUEUE_CLOSED = 1,
    QUEUE_CANCELLED = 2,
    QUEUE_FULL = 3,
    QUEUE_ERROR = -1
} queue_rc_t;

/**
 *   @brief Thread-safe blocking queue with bounded ring buffer
 *   @note Queue owns the ring buffer, not the values stored in it.
 */
typedef struct queue queue_t;

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
 * Non-blocking push. Returns QUEUE_FULL when the queue has no capacity.
 */
queue_rc_t queue_try_push(queue_t *q, void *val);

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
 * @brief Queue destroyer.
 * @note Releases queue storage only. Queued values remain caller-owned.
 * @return 0 on success.
 */
int free_queue(queue_t *q);

#endif
