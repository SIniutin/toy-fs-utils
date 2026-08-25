#ifndef UTILS_CRAWLER_H
#define UTILS_CRAWLER_H

#include <sys/stat.h>

#include <stdint.h>

#include "utils/queue.h"

// Return codes for file_processor_t
#define CRAWL_PROC_CONTINUE 0
#define CRAWL_PROC_SKIP_SUBTREE 1    // meaningful only for directories
#define CRAWL_PROC_STOP 2            // graceful stop, success
#define CRAWL_PROC_ERROR -1          // stop, error

// function to process files in crawled directories
// processor may be called concurrently when max_threads > 1
typedef int (*file_processor_t)(const char *path, const struct stat *st, void *user_data);

typedef enum
{
    CRAWL_F_REG = 1u << 0,
    CRAWL_F_DIR = 1u << 1,
    CRAWL_F_LNK = 1u << 2,
    CRAWL_F_FIFO = 1u << 3,
    CRAWL_F_SOCK = 1u << 4,
    CRAWL_F_CHR = 1u << 5,
    CRAWL_F_BLK = 1u << 6,
    CRAWL_ALL = (1u << 7) - 1u
} crawl_file_mask_t;

// config for crawler
typedef struct conf
{
    unsigned max_threads;    // >= 1; if 1 - single-threaded
    unsigned max_depth;      // >= 0; 0 - only root
    int follow_symlinks;     // if 0 - lstat, treat symlinks as type LNK
    uint32_t file_types;     // crawl_file_mask_t which files is processed
    int crawl_through;       // 1 - crawl down dirs;
} crawler_config_t;


// Crawler tasks own their path and are freed by crawler workers.
typedef struct task
{
    char *path;
    int depth;
} task_t;

// Runs crawler workers over a caller-provided queue.
int crawl_directory_q(queue_t *queue, const char *root_path, const crawler_config_t *config, file_processor_t processor, void *user_data);

int crawl_directory(const char *root_path, const crawler_config_t *config, file_processor_t processor, void *user_data);

#endif
