#define _XOPEN_SOURCE 700

#include "diskusage/stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                                              \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(expr))                                                                                                  \
        {                                                                                                             \
            fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                              \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

static int test_heap_replacement_and_ordering(void)
{
    ASSERT_TRUE(create_min_heap(0) == NULL);
    ASSERT_TRUE(create_min_heap(-1) == NULL);

    min_heap_t *heap = create_min_heap(3);
    ASSERT_TRUE(heap != NULL);

    ASSERT_TRUE(add_to_heap(heap, "medium", 20) == 0);
    ASSERT_TRUE(add_to_heap(heap, "small", 10) == 0);
    ASSERT_TRUE(add_to_heap(heap, "large", 30) == 0);
    ASSERT_TRUE(heap->count == 3);
    ASSERT_TRUE(heap->files[0].size == 10);

    ASSERT_TRUE(add_to_heap(heap, "tiny", 1) == 0);
    ASSERT_TRUE(heap->count == 3);
    ASSERT_TRUE(heap->files[0].size == 10);

    ASSERT_TRUE(add_to_heap(heap, "huge", 100) == 0);
    ASSERT_TRUE(heap->count == 3);
    ASSERT_TRUE(heap->files[0].size == 20);

    min_heap_t *manual = create_min_heap(3);
    ASSERT_TRUE(manual != NULL);
    manual->count = 3;
    manual->files[0].path = strdup("root");
    manual->files[0].size = 30;
    manual->files[1].path = strdup("left");
    manual->files[1].size = 40;
    manual->files[2].path = strdup("right");
    manual->files[2].size = 10;
    heapify_down(manual, 0);
    ASSERT_TRUE(strcmp(manual->files[0].path, "right") == 0);
    free_min_heap(manual);

    file_info_t sorted[] = {
        {.path = "b", .size = 10},
        {.path = "a", .size = 10},
        {.path = "c", .size = 30},
    };
    qsort(sorted, 3, sizeof(sorted[0]), cmp_files);
    ASSERT_TRUE(strcmp(sorted[0].path, "c") == 0);
    ASSERT_TRUE(strcmp(sorted[1].path, "a") == 0);
    ASSERT_TRUE(strcmp(sorted[2].path, "b") == 0);

    ASSERT_TRUE(add_to_heap(NULL, "x", 1) != 0);
    ASSERT_TRUE(add_to_heap(heap, NULL, 1) != 0);

    free_min_heap(NULL);
    free_min_heap(heap);
    return 0;
}

static int test_process_file_stats(void)
{
    min_heap_t *heap = create_min_heap(4);
    ASSERT_TRUE(heap != NULL);

    stats_t stats = {0};
    ASSERT_TRUE(pthread_mutex_init(&stats.mutex, NULL) == 0);
    stats.top_files = heap;

    struct stat st = {0};
    st.st_mode = S_IFREG | 0600;
    st.st_size = 7;

    ASSERT_TRUE(process_file("plain", &st, &stats) == 0);
    ASSERT_TRUE(process_file("/tmp/.hidden", &st, &stats) == 0);
    ASSERT_TRUE(process_file("/tmp/app.log", &st, &stats) == 0);
    ASSERT_TRUE(process_file("/tmp/cache.tmp", &st, &stats) == 0);

    ASSERT_TRUE(stats.files_count == 4);
    ASSERT_TRUE(stats.total_size == 28);
    ASSERT_TRUE(stats.hidden_size == 7);
    ASSERT_TRUE(stats.logs_size == 7);
    ASSERT_TRUE(stats.tmp_size == 7);

    struct stat dir_st = {0};
    dir_st.st_mode = S_IFDIR | 0700;
    ASSERT_TRUE(process_file("/tmp/dir", &dir_st, &stats) == 0);
    ASSERT_TRUE(stats.files_count == 4);

    ASSERT_TRUE(process_file(NULL, &st, &stats) != 0);
    ASSERT_TRUE(process_file("plain", NULL, &stats) != 0);
    ASSERT_TRUE(process_file("plain", &st, NULL) != 0);

    free_min_heap(heap);
    pthread_mutex_destroy(&stats.mutex);

    stats_t no_heap = {0};
    ASSERT_TRUE(pthread_mutex_init(&no_heap.mutex, NULL) == 0);
    ASSERT_TRUE(process_file("warn.log", &st, &no_heap) == 0);
    ASSERT_TRUE(no_heap.files_count == 1);
    ASSERT_TRUE(no_heap.logs_size == 7);
    pthread_mutex_destroy(&no_heap.mutex);

    return 0;
}

int main(void)
{
    if (test_heap_replacement_and_ordering() != 0)
        return 1;
    if (test_process_file_stats() != 0)
        return 1;
    return 0;
}
