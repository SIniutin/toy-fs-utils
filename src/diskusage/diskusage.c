#define _XOPEN_SOURCE 700

#include "diskusage/stats.h"
#include "utils/crawler.h"
#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 4
#define TOP_COUNT 10

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        puts("usage: ./diskusage <path/to/dir>");
        return 1;
    }

    struct stat path_stat;
    if (stat(argv[1], &path_stat) != 0)
    {
        fprintf(stderr, "diskusage: error cannot access '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    if (!S_ISDIR(path_stat.st_mode))
    {
        fprintf(stderr, "diskusage: error '%s' is not a directory\n", argv[1]);
        return 1;
    }

    stats_t *stats = malloc(sizeof(stats_t));
    if (!stats)
    {
        fprintf(stderr, "diskusage: memory allocation failed\n");
        return 2;
    }

    if (pthread_mutex_init(&stats->mutex, NULL) != 0)
    {
        fprintf(stderr, "diskusage: mutex initialization failed\n");
        free(stats);
        return 2;
    }

    stats->files_count = 0;
    stats->hidden_size = 0;
    stats->logs_size = 0;
    stats->tmp_size = 0;
    stats->total_size = 0;
    stats->top_files = create_min_heap(TOP_COUNT);

    if (!stats->top_files)
    {
        fputs("diskusage: allocation failed", stderr);
        pthread_mutex_destroy(&stats->mutex);
        free(stats);
        return 2;
    }

    const char *root = argv[1];
    crawler_config_t config = { .max_threads = NUM_THREADS, .max_depth = UINT_MAX, .follow_symlinks = 0, .file_types = CRAWL_F_REG, .crawl_through = 1 };

    if (crawl_directory(root, &config, process_file, stats) != 0)
    {
        fputs("diskusage: crawling failed", stderr);
        free_min_heap(stats->top_files);
        pthread_mutex_destroy(&stats->mutex);
        free(stats);
        return 3;
    }
    char timebuf[32];
    time_t rawtime = time(NULL);
    struct tm *timeinfo = localtime(&rawtime);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", timeinfo);

    printf("=== Disk Report | %s ===\n", timebuf);
    printf("Files: %llu\n", stats->files_count);

    printf("Top-10:\n");
    file_info_t *sorted = malloc(sizeof(file_info_t) * stats->top_files->count);
    if (sorted)
    {
        pthread_mutex_lock(&stats->top_files->mutex);
        memcpy(sorted, stats->top_files->files, sizeof(file_info_t) * stats->top_files->count);
        pthread_mutex_unlock(&stats->top_files->mutex);

        qsort(sorted, stats->top_files->count, sizeof(file_info_t), cmp_files);

        for (int i = 0; i < stats->top_files->count; i++)
        {
            printf("  %d. %s (%llu B)\n", i + 1, sorted[i].path, sorted[i].size);
        }
        free(sorted);
    }

    printf("Logs: %llu B\n", stats->logs_size);
    printf("Temp: %llu B\n", stats->tmp_size);
    printf("Hidden: %llu B\n", stats->hidden_size);
    printf("---------------------------------------\n");
    printf("Total: %llu B\n", stats->total_size);
    printf("=======================================\n");
    printf("||        prod. by * p0tniy_Zadr     ||\n");
    printf("=======================================\n");

    free_min_heap(stats->top_files);
    pthread_mutex_destroy(&stats->mutex);
    free(stats);

    return 0;
}
