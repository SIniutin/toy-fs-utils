#ifndef DISKUSAGE_STATS_H
#define DISKUSAGE_STATS_H

#include <pthread.h>
#include <sys/stat.h>

typedef unsigned long long diskusage_ull_t;

typedef struct
{
    char *path;
    diskusage_ull_t size;
} file_info_t;

typedef struct
{
    file_info_t *files;
    int count;
    int capacity;
    pthread_mutex_t mutex;
} min_heap_t;

typedef struct
{
    diskusage_ull_t files_count;
    diskusage_ull_t total_size;
    diskusage_ull_t hidden_size;
    diskusage_ull_t tmp_size;
    diskusage_ull_t logs_size;
    pthread_mutex_t mutex;
    min_heap_t *top_files;
} stats_t;

min_heap_t *create_min_heap(int capacity);
void free_min_heap(min_heap_t *heap);
void heap_swap(file_info_t *a, file_info_t *b);
void heapify_up(min_heap_t *heap, int index);
void heapify_down(min_heap_t *heap, int index);
int add_to_heap(min_heap_t *heap, const char *path, diskusage_ull_t size);
int cmp_files(const void *a, const void *b);
int process_file(const char *path, const struct stat *st, void *user_data);

#endif
