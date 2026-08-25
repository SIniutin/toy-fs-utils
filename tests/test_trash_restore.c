#define _XOPEN_SOURCE 700

#include "trashbin/restore.h"
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

static int write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    return fclose(f);
}

static int test_format_deleted_time(void)
{
    char buf[64];

    trash_restore_format_deleted_time(NULL, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "?") == 0);

    trash_restore_format_deleted_time("2026-08-24_15-01-02", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2026-08-24 15:01:02") == 0);

    trash_restore_format_deleted_time("20260824150102", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "2026-08-24 15:01:02") == 0);

    trash_restore_format_deleted_time("short", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "short") == 0);

    return 0;
}

static int test_get_dirname_and_unique_name(void)
{
    char root[] = "/tmp/toyfs-untrash-unit.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char buf[PATH_MAX];
    trash_restore_get_dirname("plain.txt", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, ".") == 0);

    trash_restore_get_dirname("/tmp/plain.txt", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "/tmp") == 0);

    char existing[PATH_MAX];
    ASSERT_TRUE(join_path_into(existing, sizeof(existing), root, "file") == 0);
    ASSERT_TRUE(write_text(existing, "old") == 0);

    char unique[PATH_MAX];
    ASSERT_TRUE(trash_restore_make_unique_name(root, "file", unique, sizeof(unique)) == 0);
    ASSERT_TRUE(strstr(unique, "file(1)") != NULL);

    ASSERT_TRUE(trash_restore_make_unique_name(root, "name.txt", unique, sizeof(unique)) == 0);
    ASSERT_TRUE(strstr(unique, "name(1).txt") != NULL);

    return 0;
}

static int test_restore_file_rename_path(void)
{
    char root[] = "/tmp/toyfs-untrash-restore.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char src[PATH_MAX];
    char dst[PATH_MAX];
    ASSERT_TRUE(join_path_into(src, sizeof(src), root, "src.txt") == 0);
    ASSERT_TRUE(join_path_into(dst, sizeof(dst), root, "dst.txt") == 0);
    ASSERT_TRUE(write_text(src, "payload") == 0);

    ASSERT_TRUE(trash_restore_file(src, dst) == 0);
    ASSERT_TRUE(access(src, F_OK) != 0);

    FILE *f = fopen(dst, "r");
    ASSERT_TRUE(f != NULL);
    char buf[32];
    ASSERT_TRUE(fgets(buf, sizeof(buf), f) != NULL);
    fclose(f);
    ASSERT_TRUE(strcmp(buf, "payload") == 0);

    ASSERT_TRUE(trash_restore_file(src, dst) != 0);
    return 0;
}

static int test_prepare_dest_dir(void)
{
    char home[] = "/tmp/toyfs-untrash-home.XXXXXX";
    ASSERT_TRUE(mkdtemp(home) != NULL);

    char original_dir[PATH_MAX];
    ASSERT_TRUE(join_path_into(original_dir, sizeof(original_dir), home, "original") == 0);
    ASSERT_TRUE(mkdir(original_dir, 0700) == 0);

    char original[PATH_MAX];
    ASSERT_TRUE(join_path_into(original, sizeof(original), original_dir, "file.txt") == 0);

    char dest[PATH_MAX];
    ASSERT_TRUE(trash_restore_prepare_dest_dir(original, "", home, dest, sizeof(dest)) == 0);
    ASSERT_TRUE(strcmp(dest, original_dir) == 0);

    char explicit_dir[PATH_MAX];
    ASSERT_TRUE(join_path_into(explicit_dir, sizeof(explicit_dir), home, "custom/nested") == 0);
    ASSERT_TRUE(trash_restore_prepare_dest_dir(original, explicit_dir, home, dest, sizeof(dest)) == 0);
    ASSERT_TRUE(strcmp(dest, explicit_dir) == 0);
    ASSERT_TRUE(access(explicit_dir, F_OK) == 0);

    char missing_original[PATH_MAX];
    ASSERT_TRUE(join_path_into(missing_original, sizeof(missing_original), home, "missing/file.txt") == 0);
    ASSERT_TRUE(trash_restore_prepare_dest_dir(missing_original, "", home, dest, sizeof(dest)) == 0);
    ASSERT_TRUE(strstr(dest, "restore-lost") != NULL);
    ASSERT_TRUE(access(dest, F_OK) == 0);

    ASSERT_TRUE(trash_restore_prepare_dest_dir(NULL, "", home, dest, sizeof(dest)) != 0);
    ASSERT_TRUE(trash_restore_prepare_dest_dir(original, "", NULL, dest, sizeof(dest)) != 0);
    ASSERT_TRUE(trash_restore_prepare_dest_dir(original, "", home, NULL, sizeof(dest)) != 0);

    return 0;
}

static int test_prepare_dest_path(void)
{
    char root[] = "/tmp/toyfs-untrash-dest.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char dest[PATH_MAX];
    ASSERT_TRUE(trash_restore_prepare_dest_path(root, "plain.txt", 0, 0, dest, sizeof(dest)) == TRASH_RESTORE_DEST_OK);
    ASSERT_TRUE(strstr(dest, "plain.txt") != NULL);

    ASSERT_TRUE(write_text(dest, "old") == 0);
    ASSERT_TRUE(trash_restore_prepare_dest_path(root, "plain.txt", 0, 0, dest, sizeof(dest)) ==
                TRASH_RESTORE_DEST_EXISTS);
    ASSERT_TRUE(access(dest, F_OK) == 0);

    ASSERT_TRUE(trash_restore_prepare_dest_path(root, "plain.txt", 1, 0, dest, sizeof(dest)) == TRASH_RESTORE_DEST_OK);
    ASSERT_TRUE(access(dest, F_OK) != 0);

    ASSERT_TRUE(write_text(dest, "old") == 0);
    char unique[PATH_MAX];
    ASSERT_TRUE(trash_restore_prepare_dest_path(root, "plain.txt", 0, 1, unique, sizeof(unique)) ==
                TRASH_RESTORE_DEST_OK);
    ASSERT_TRUE(strstr(unique, "plain(1).txt") != NULL);

    ASSERT_TRUE(trash_restore_prepare_dest_path(NULL, "plain.txt", 0, 0, dest, sizeof(dest)) ==
                TRASH_RESTORE_DEST_ERROR);
    ASSERT_TRUE(trash_restore_prepare_dest_path(root, NULL, 0, 0, dest, sizeof(dest)) == TRASH_RESTORE_DEST_ERROR);
    ASSERT_TRUE(trash_restore_prepare_dest_path(root, "plain.txt", 0, 0, NULL, sizeof(dest)) ==
                TRASH_RESTORE_DEST_ERROR);

    return 0;
}

int main(void)
{
    if (test_format_deleted_time() != 0)
        return 1;
    if (test_get_dirname_and_unique_name() != 0)
        return 1;
    if (test_restore_file_rename_path() != 0)
        return 1;
    if (test_prepare_dest_dir() != 0)
        return 1;
    if (test_prepare_dest_path() != 0)
        return 1;
    return 0;
}
