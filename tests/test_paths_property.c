#define _XOPEN_SOURCE 700

#include "archive/restore.h"
#include "utils/common.h"

#include <stdint.h>
#include <stdio.h>
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

static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void make_component(char *out, size_t len, uint32_t *state)
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789_-";
    for (size_t i = 0; i < len; ++i)
        out[i] = alphabet[next_random(state) % (sizeof(alphabet) - 1)];
    out[len] = '\0';
}

static int test_join_path_properties(void)
{
    uint32_t state = 0x5eed1234u;

    for (size_t iteration = 0; iteration < 10000; ++iteration)
    {
        char parent[48];
        char child[48];
        size_t parent_len = 1 + next_random(&state) % 40;
        size_t child_len = 1 + next_random(&state) % 40;
        make_component(parent, parent_len, &state);
        make_component(child, child_len, &state);

        char expected[128];
        int expected_len = snprintf(expected, sizeof(expected), "%s/%s", parent, child);
        ASSERT_TRUE(expected_len > 0 && (size_t)expected_len < sizeof(expected));

        char out[128];
        ASSERT_TRUE(join_path_into(out, sizeof(out), parent, child) == 0);
        ASSERT_TRUE(strcmp(out, expected) == 0);
        ASSERT_TRUE(strcmp(get_basename(out), child) == 0);

        size_t exact_size = (size_t)expected_len + 1;
        ASSERT_TRUE(join_path_into(out, exact_size, parent, child) == 0);
        ASSERT_TRUE(strcmp(out, expected) == 0);

        memset(out, 'X', sizeof(out));
        ASSERT_TRUE(join_path_into(out, (size_t)expected_len, parent, child) != 0);
        ASSERT_TRUE(out[0] == '\0');
    }

    return 0;
}

static int test_restore_path_boundaries(void)
{
    static const char *unsafe[] = {
        "",
        ".",
        "..",
        "../escape",
        "dir/../escape",
        "dir/./file",
        "dir//file",
        "dir/",
        "/absolute",
        ".versions",
        ".versions/file",
        ".source_path",
        "versions.tar.gz",
    };

    char out[128];
    for (size_t i = 0; i < sizeof(unsafe) / sizeof(unsafe[0]); ++i)
        ASSERT_TRUE(archive_restore_dest_path("/restore", unsafe[i], out, sizeof(out)) != 0);

    ASSERT_TRUE(archive_restore_dest_path("/restore", "dir/file.txt", out, sizeof(out)) == 0);
    ASSERT_TRUE(strcmp(out, "/restore/dir/file.txt") == 0);
    ASSERT_TRUE(archive_restore_dest_path("/restore", "dir/file.txt", out, 8) != 0);
    ASSERT_TRUE(out[0] == '\0');

    return 0;
}

int main(void)
{
    if (test_join_path_properties() != 0)
        return 1;
    if (test_restore_path_boundaries() != 0)
        return 1;
    return 0;
}
