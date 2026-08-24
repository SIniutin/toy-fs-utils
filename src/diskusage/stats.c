#define _XOPEN_SOURCE 700

#include "diskusage/stats.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

min_heap_t *create_min_heap(int capacity)
{
    if (capacity <= 0)
        return NULL;

    min_heap_t *heap = malloc(sizeof(min_heap_t));
    if (!heap)
        return NULL;

    heap->files = malloc(sizeof(file_info_t) * capacity);
    if (!heap->files)
    {
        free(heap);
        return NULL;
    }

    heap->count = 0;
    heap->capacity = capacity;
    if (pthread_mutex_init(&heap->mutex, NULL) != 0)
    {
        free(heap->files);
        free(heap);
        return NULL;
    }

    return heap;
}

void free_min_heap(min_heap_t *heap)
{
    if (!heap)
        return;

    pthread_mutex_lock(&heap->mutex);
    for (int i = 0; i < heap->count; i++)
    {
        free(heap->files[i].path);
    }
    free(heap->files);
    pthread_mutex_unlock(&heap->mutex);

    pthread_mutex_destroy(&heap->mutex);
    free(heap);
}

void heap_swap(file_info_t *a, file_info_t *b)
{
    file_info_t temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(min_heap_t *heap, int index)
{
    while (index > 0)
    {
        int parent = (index - 1) / 2;
        if (heap->files[parent].size <= heap->files[index].size)
            break;
        heap_swap(&heap->files[parent], &heap->files[index]);
        index = parent;
    }
}

void heapify_down(min_heap_t *heap, int index)
{
    while (1)
    {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;

        if (left < heap->count && heap->files[left].size < heap->files[smallest].size)
            smallest = left;
        if (right < heap->count && heap->files[right].size < heap->files[smallest].size)
            smallest = right;

        if (smallest == index)
            break;
        heap_swap(&heap->files[index], &heap->files[smallest]);
        index = smallest;
    }
}

int add_to_heap(min_heap_t *heap, const char *path, diskusage_ull_t size)
{
    if (!heap || !path)
        return 1;

    pthread_mutex_lock(&heap->mutex);

    if (heap->count < heap->capacity)
    {
        char *copy = strdup(path);
        if (!copy)
        {
            pthread_mutex_unlock(&heap->mutex);
            return 1;
        }
        heap->files[heap->count].path = copy;
        heap->files[heap->count].size = size;
        heapify_up(heap, heap->count);
        heap->count++;
    }
    else if (size > heap->files[0].size)
    {
        char *copy = strdup(path);
        if (!copy)
        {
            pthread_mutex_unlock(&heap->mutex);
            return 1;
        }
        free(heap->files[0].path);
        heap->files[0].path = copy;
        heap->files[0].size = size;
        heapify_down(heap, 0);
    }

    pthread_mutex_unlock(&heap->mutex);
    return 0;
}

int cmp_files(const void *a, const void *b)
{
    const file_info_t *x = a;
    const file_info_t *y = b;
    if (x->size != y->size)
        return (x->size < y->size) ? 1 : -1;
    return strcmp(x->path, y->path);
}

int process_file(const char *path, const struct stat *st, void *user_data)
{
    if (!path || !st || !user_data)
    {
        return -1;
    }

    if (!S_ISREG(st->st_mode))
        return 0;

    stats_t *stats = (stats_t *)user_data;
    pthread_mutex_lock(&stats->mutex);
    stats->files_count++;
    stats->total_size += (diskusage_ull_t)st->st_size;
    pthread_mutex_unlock(&stats->mutex);

    const char *n = strrchr(path, '/');
    if (n)
        n++;
    else
        n = path;

    if (n[0] == '.')
    {
        pthread_mutex_lock(&stats->mutex);
        stats->hidden_size += (uint64_t)st->st_size;
        pthread_mutex_unlock(&stats->mutex);
    }

    size_t len = strlen(n);
    if (len >= 4)
    {
        pthread_mutex_lock(&stats->mutex);
        if (strcmp(n + len - 4, ".log") == 0)
        {
            stats->logs_size += (uint64_t)st->st_size;
        }
        else if (strcmp(n + len - 4, ".tmp") == 0)
        {
            stats->tmp_size += (uint64_t)st->st_size;
        }
        pthread_mutex_unlock(&stats->mutex);
    }

    if (add_to_heap(stats->top_files, path, (uint64_t)st->st_size) != 0)
    {
        fprintf(stderr, "Warning: Failed to add file to heap: %s\n", path);
    }

    return 0;
}
