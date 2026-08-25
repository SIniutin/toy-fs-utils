#define _XOPEN_SOURCE 700

#include "utils/common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ASSERT_TRUE(expr)                                                                                              \
    do                                                                                                                \
    {                                                                                                                 \
        if (!(expr))                                                                                                  \
        {                                                                                                             \
            fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                              \
            return 1;                                                                                                 \
        }                                                                                                             \
    } while (0)

static int test_datetime(void)
{
    char buf[32];
    get_current_datetime(buf, sizeof(buf));
    ASSERT_TRUE(strlen(buf) == 19);

    get_current_datetime(NULL, sizeof(buf));
    get_current_datetime(buf, 0);
    return 0;
}

static int test_path_helpers(void)
{
    char buf[PATH_MAX];

    ASSERT_TRUE(join_path_into(buf, sizeof(buf), "/tmp", "file") == 0);
    ASSERT_TRUE(strcmp(buf, "/tmp/file") == 0);
    ASSERT_TRUE(snprintf_checked(buf, sizeof(buf), "%s", "abc") == 0);
    ASSERT_TRUE(strcmp(buf, "abc") == 0);

    ASSERT_TRUE(join_path_into(buf, 4, "/tmp", "file") != 0);
    ASSERT_TRUE(snprintf_checked(buf, 4, "%s", "abcdef") != 0);
    ASSERT_TRUE(snprintf_checked(NULL, sizeof(buf), "%s", "abcdef") != 0);
    ASSERT_TRUE(snprintf_checked(buf, 0, "%s", "abcdef") != 0);
    ASSERT_TRUE(snprintf_checked(buf, sizeof(buf), NULL) != 0);

    char *joined = join_path("/tmp", "joined");
    ASSERT_TRUE(joined != NULL);
    ASSERT_TRUE(strcmp(joined, "/tmp/joined") == 0);
    free(joined);

    ASSERT_TRUE(strcmp(get_basename("/tmp/name.txt"), "name.txt") == 0);
    ASSERT_TRUE(strcmp(get_basename("plain"), "plain") == 0);
    return 0;
}

static int test_mkdir_p(void)
{
    char root[] = "/tmp/toyfs-common.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char nested[PATH_MAX];
    ASSERT_TRUE(join_path_into(nested, sizeof(nested), root, "a/b/c") == 0);
    ASSERT_TRUE(mkdir_p(nested, 0700) == 0);

    struct stat st;
    ASSERT_TRUE(stat(nested, &st) == 0);
    ASSERT_TRUE(S_ISDIR(st.st_mode));
    ASSERT_TRUE(mkdir_p(nested, 0700) == 0);
    ASSERT_TRUE(mkdir_p("", 0700) != 0);
    ASSERT_TRUE(mkdir_p(nested, 0700) == 0);

    char file_path[PATH_MAX];
    ASSERT_TRUE(join_path_into(file_path, sizeof(file_path), root, "file") == 0);
    FILE *f = fopen(file_path, "w");
    ASSERT_TRUE(f != NULL);
    fclose(f);
    ASSERT_TRUE(mkdir_p(file_path, 0700) != 0);

    return 0;
}

int main(void)
{
    if (test_datetime() != 0)
        return 1;
    if (test_path_helpers() != 0)
        return 1;
    if (test_mkdir_p() != 0)
        return 1;
    return 0;
}
