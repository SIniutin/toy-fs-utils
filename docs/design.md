# Design Notes

This document records the core invariants for the shared queue and crawler.

## Queue Contract

`queue_t` is opaque. Callers interact with it only through the queue API.

The queue owns:

- its ring buffer;
- synchronization primitives;
- queue state.

The queue does not own queued values. A value pushed into the queue remains
caller-owned. `free_queue` releases queue storage only; it never frees queued
values.

Lifecycle:

- `queue_push` blocks while the queue is full.
- `queue_try_push` returns `QUEUE_FULL` instead of blocking.
- `queue_close` rejects new pushes and lets consumers drain existing values.
- `queue_cancel` wakes waiters and makes push/pop return `QUEUE_CANCELLED`.
- `free_queue` is valid after normal drain, close, or cancel.

## Crawler Task Ownership

Each `task_t` owns its `path`.

Ownership rules:

- queued crawler tasks are allocated by the crawler;
- worker threads free each popped task exactly once;
- if a task cannot be queued, the code path that still owns it frees it;
- caller-provided paths are copied before they become crawler tasks.

## Active Task Counter

`active_tasks` counts directory tasks that are queued or currently being
processed by workers.

Invariant:

```text
active_tasks == queued directory tasks + directory tasks currently owned by workers
```

The root task starts with `active_tasks = 1`.

When a worker creates a queued child directory task:

1. increment `active_tasks`;
2. attempt to enqueue the task;
3. if enqueue fails, decrement `active_tasks` and release/process the task
   according to the error path.

When a worker finishes a popped directory task:

1. free the task;
2. decrement `active_tasks`;
3. if it reaches zero, close the queue.

## Bounded Queue Progress

Crawler workers must not all block while pushing child directory tasks into a
full queue. With a small bounded queue, that can deadlock because every worker
may become a producer with no remaining consumer.

Current rule:

- workers use `queue_try_push` for child directory tasks;
- if the queue is full, the worker processes that child directory inline;
- this preserves progress while keeping the normal queued fast path.

## Shutdown Sequence

Normal completion:

1. root task is queued;
2. workers process directory tasks;
3. the last completed active task closes the queue;
4. workers drain and exit;
5. `crawl_directory_q` joins all workers and releases crawler state.

Callback stop:

- `CRAWL_PROC_STOP` requests a graceful stop and returns success.

Callback error:

- `CRAWL_PROC_ERROR` requests a stop and returns failure.

Both stop paths close the queue to wake blocked workers.

## Symlink Cycle Detection

When `follow_symlinks` is enabled, a directory symlink can point back to an
ancestor and create a traversal cycle.

The crawler tracks visited directories by:

```text
(st_dev, st_ino)
```

If a directory identity was already seen, the crawler skips traversing it again.
Directory entries may still be reported to the callback according to
`file_types`, but repeated directory identities are not recursively expanded.
