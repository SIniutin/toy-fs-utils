#define _XOPEN_SOURCE 700

#include "archive/file_ops.h"
#include "archive/paths.h"
#include "archive/restore.h"
#include "archive/snapshot.h"
#include "utils/common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

static int test_archive_paths(void)
{
    char home[] = "/tmp/toyfs-archive-home.XXXXXX";
    ASSERT_TRUE(mkdtemp(home) != NULL);
    ASSERT_TRUE(setenv("HOME", home, 1) == 0);

    char path[PATH_MAX];
    ASSERT_TRUE(archive_backups_dir(path, sizeof(path)) == 0);
    ASSERT_TRUE(strstr(path, "/Backups") != NULL);

    ASSERT_TRUE(archive_backup_dir(path, sizeof(path), "sample-2026-08-24") == 0);
    ASSERT_TRUE(strstr(path, "/Backups/sample-2026-08-24") != NULL);

    ASSERT_TRUE(archive_versions_dir(path, sizeof(path), "/tmp/backup") == 0);
    ASSERT_TRUE(strcmp(path, "/tmp/backup/.versions") == 0);

    ASSERT_TRUE(archive_source_path_file(path, sizeof(path), "/tmp/backup") == 0);
    ASSERT_TRUE(strcmp(path, "/tmp/backup/.source_path") == 0);

    ASSERT_TRUE(unsetenv("HOME") == 0);
    ASSERT_TRUE(archive_backups_dir(path, sizeof(path)) != 0);
    ASSERT_TRUE(archive_backup_dir(path, sizeof(path), "sample") != 0);
    ASSERT_TRUE(setenv("HOME", home, 1) == 0);

    return 0;
}

static int test_file_ops(void)
{
    char root[] = "/tmp/toyfs-archive.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char buf[PATH_MAX];
    archive_dirname("plain.txt", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, ".") == 0);

    archive_dirname("/tmp/plain.txt", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "/tmp") == 0);

    archive_dirname("/plain.txt", buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "") == 0);

    char tiny[4];
    archive_dirname("/abc/def", tiny, sizeof(tiny));
    ASSERT_TRUE(strcmp(tiny, "/ab") == 0);

    char meta[PATH_MAX];
    ASSERT_TRUE(join_path_into(meta, sizeof(meta), root, ".source_path") == 0);
    ASSERT_TRUE(archive_write_text_file_atomic(meta, "SOURCE=/tmp/source\n", "test_archive") == 0);

    char source[PATH_MAX];
    ASSERT_TRUE(archive_read_source_path(root, source, sizeof(source)) == 0);
    ASSERT_TRUE(strcmp(source, "/tmp/source") == 0);

    ASSERT_TRUE(archive_read_source_path(root, source, 4) != 0);

    ASSERT_TRUE(archive_write_text_file_atomic(meta, "/tmp/plain-source\n", "test_archive") == 0);
    ASSERT_TRUE(archive_read_source_path(root, source, sizeof(source)) == 0);
    ASSERT_TRUE(strcmp(source, "/tmp/plain-source") == 0);

    ASSERT_TRUE(archive_write_text_file_atomic(meta, "SOURCE=\n", "test_archive") == 0);
    ASSERT_TRUE(archive_read_source_path(root, source, sizeof(source)) != 0);

    ASSERT_TRUE(archive_write_text_file_atomic(meta, "", "test_archive") == 0);
    ASSERT_TRUE(archive_read_source_path(root, source, sizeof(source)) != 0);

    char src[PATH_MAX];
    char dst[PATH_MAX];
    ASSERT_TRUE(join_path_into(src, sizeof(src), root, "src.txt") == 0);
    ASSERT_TRUE(join_path_into(dst, sizeof(dst), root, "dst.txt") == 0);
    ASSERT_TRUE(write_text(src, "payload") == 0);

    struct stat st;
    ASSERT_TRUE(stat(src, &st) == 0);
    ASSERT_TRUE(archive_copy_file(src, dst, &st, "test_archive") == 0);

    FILE *f = fopen(dst, "r");
    ASSERT_TRUE(f != NULL);
    char text[32];
    ASSERT_TRUE(fgets(text, sizeof(text), f) != NULL);
    fclose(f);
    ASSERT_TRUE(strcmp(text, "payload") == 0);

    struct stat dst_st;
    ASSERT_TRUE(stat(dst, &dst_st) == 0);
    ASSERT_TRUE(dst_st.st_mtime == st.st_mtime);

    char dst_no_stat[PATH_MAX];
    ASSERT_TRUE(join_path_into(dst_no_stat, sizeof(dst_no_stat), root, "dst-no-stat.txt") == 0);
    ASSERT_TRUE(archive_copy_file(src, dst_no_stat, NULL, "test_archive") == 0);
    ASSERT_TRUE(access(dst_no_stat, F_OK) == 0);

    char bad_dst[PATH_MAX];
    ASSERT_TRUE(join_path_into(bad_dst, sizeof(bad_dst), root, "missing-dir/out.txt") == 0);
    ASSERT_TRUE(archive_copy_file(src, bad_dst, NULL, "test_archive") != 0);

    ASSERT_TRUE(archive_copy_file("/tmp/toyfs-missing-file", dst, NULL, "test_archive") != 0);
    return 0;
}

static int test_file_ops_failures(void)
{
    char root[] = "/tmp/toyfs-archive-failures.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char source[PATH_MAX];
    ASSERT_TRUE(archive_read_source_path(root, source, sizeof(source)) != 0);

    char dir_path[PATH_MAX];
    ASSERT_TRUE(join_path_into(dir_path, sizeof(dir_path), root, "dir") == 0);
    ASSERT_TRUE(mkdir(dir_path, 0700) == 0);
    ASSERT_TRUE(archive_write_text_file_atomic(dir_path, "payload", "test_archive") != 0);

    return 0;
}

static int test_archive_snapshot_status(void)
{
    char root[] = "/tmp/toyfs-archive-snapshot.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char src[PATH_MAX];
    char same[PATH_MAX];
    char changed[PATH_MAX];
    char missing[PATH_MAX];
    ASSERT_TRUE(join_path_into(src, sizeof(src), root, "src.txt") == 0);
    ASSERT_TRUE(join_path_into(same, sizeof(same), root, "same.txt") == 0);
    ASSERT_TRUE(join_path_into(changed, sizeof(changed), root, "changed.txt") == 0);
    ASSERT_TRUE(join_path_into(missing, sizeof(missing), root, "missing.txt") == 0);

    ASSERT_TRUE(write_text(src, "payload") == 0);
    ASSERT_TRUE(write_text(same, "payload") == 0);
    ASSERT_TRUE(write_text(changed, "different") == 0);

    struct stat st_src;
    struct stat st_same;
    struct stat st_changed;
    ASSERT_TRUE(stat(src, &st_src) == 0);
    ASSERT_TRUE(stat(same, &st_same) == 0);
    ASSERT_TRUE(stat(changed, &st_changed) == 0);

    st_same.st_mtime = st_src.st_mtime;
    ASSERT_TRUE(archive_file_is_unchanged(&st_src, &st_src) == 1);
    ASSERT_TRUE(archive_file_is_unchanged(&st_src, &st_same) == 1);
    ASSERT_TRUE(archive_file_is_unchanged(&st_src, &st_changed) == 0);
    ASSERT_TRUE(archive_file_is_unchanged(NULL, &st_changed) == 0);
    ASSERT_TRUE(archive_file_is_unchanged(&st_src, NULL) == 0);

    ASSERT_TRUE(archive_compare_with_dst(&st_src, src, "test_archive") == ARCHIVE_FILE_UNCHANGED);
    ASSERT_TRUE(archive_compare_with_dst(&st_src, changed, "test_archive") == ARCHIVE_FILE_CHANGED);
    ASSERT_TRUE(archive_compare_with_dst(&st_src, missing, "test_archive") == ARCHIVE_FILE_NEW);

    return 0;
}

static int test_archive_version_path(void)
{
    char root[] = "/tmp/toyfs-archive-version.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char versions[PATH_MAX];
    ASSERT_TRUE(archive_versions_dir(versions, sizeof(versions), root) == 0);
    ASSERT_TRUE(mkdir(versions, 0700) == 0);

    char out[PATH_MAX];
    ASSERT_TRUE(archive_version_path(root, "file.txt", "2026-08-24_15-01-02", out, sizeof(out)) == 0);
    ASSERT_TRUE(strstr(out, "/.versions/file.txt@2026-08-24_15-01-02") != NULL);

    ASSERT_TRUE(write_text(out, "first") == 0);
    ASSERT_TRUE(archive_version_path(root, "file.txt", "2026-08-24_15-01-02", out, sizeof(out)) == 0);
    ASSERT_TRUE(strstr(out, "/.versions/file.txt@2026-08-24_15-01-02.1") != NULL);

    char nested[PATH_MAX];
    ASSERT_TRUE(archive_version_path(root, "dir/file.txt", "2026-08-24_15-01-02", nested, sizeof(nested)) == 0);
    ASSERT_TRUE(strstr(nested, "/.versions/dir/file.txt@2026-08-24_15-01-02") != NULL);

    ASSERT_TRUE(archive_version_path(root, "file.txt", "2026-08-24_15-01-02", out, 4) != 0);

    return 0;
}

static int test_archive_restore_rules(void)
{
    ASSERT_TRUE(archive_restore_should_skip_rel(NULL) == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel("") == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel(".versions") == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel(".versions/file.txt") == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel("versions.tar.gz") == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel(".source_path") == 1);
    ASSERT_TRUE(archive_restore_should_skip_rel("file.txt") == 0);
    ASSERT_TRUE(archive_restore_should_skip_rel("dir/.source_path") == 0);

    char dest[PATH_MAX];
    ASSERT_TRUE(archive_restore_dest_path("/tmp/root", "dir/file.txt", dest, sizeof(dest)) == 0);
    ASSERT_TRUE(strcmp(dest, "/tmp/root/dir/file.txt") == 0);
    ASSERT_TRUE(archive_restore_dest_path("/tmp/root", ".source_path", dest, sizeof(dest)) != 0);
    ASSERT_TRUE(archive_restore_dest_path(NULL, "file.txt", dest, sizeof(dest)) != 0);
    ASSERT_TRUE(archive_restore_dest_path("/tmp/root", NULL, dest, sizeof(dest)) != 0);
    ASSERT_TRUE(archive_restore_dest_path("/tmp/root", "file.txt", NULL, sizeof(dest)) != 0);

    return 0;
}

static int test_archive_restore_dest_dirs(void)
{
    char root[] = "/tmp/toyfs-archive-restore.XXXXXX";
    ASSERT_TRUE(mkdtemp(root) != NULL);

    char backup_root[PATH_MAX];
    ASSERT_TRUE(join_path_into(backup_root, sizeof(backup_root), root, "backup") == 0);
    ASSERT_TRUE(mkdir(backup_root, 0700) == 0);

    char source_dir[PATH_MAX];
    ASSERT_TRUE(join_path_into(source_dir, sizeof(source_dir), root, "source") == 0);

    char source_file[PATH_MAX];
    ASSERT_TRUE(archive_source_path_file(source_file, sizeof(source_file), backup_root) == 0);

    char line[PATH_MAX + 16];
    ASSERT_TRUE(snprintf_checked(line, sizeof(line), "SOURCE=%s\n", source_dir) == 0);
    ASSERT_TRUE(archive_write_text_file_atomic(source_file, line, "test_archive") == 0);

    char out[PATH_MAX];
    ASSERT_TRUE(archive_restore_resolve_dest_root(backup_root, "", out, sizeof(out)) == 0);
    ASSERT_TRUE(strcmp(out, source_dir) == 0);
    ASSERT_TRUE(access(source_dir, F_OK) == 0);

    char explicit_dir[PATH_MAX];
    ASSERT_TRUE(join_path_into(explicit_dir, sizeof(explicit_dir), root, "custom/nested") == 0);
    ASSERT_TRUE(archive_restore_resolve_dest_root(backup_root, explicit_dir, out, sizeof(out)) == 0);
    ASSERT_TRUE(strcmp(out, explicit_dir) == 0);
    ASSERT_TRUE(access(explicit_dir, F_OK) == 0);

    char nested_file[PATH_MAX];
    ASSERT_TRUE(join_path_into(nested_file, sizeof(nested_file), root, "parent/child/file.txt") == 0);
    ASSERT_TRUE(archive_restore_ensure_parent_dir(nested_file) == 0);
    char parent[PATH_MAX];
    ASSERT_TRUE(join_path_into(parent, sizeof(parent), root, "parent/child") == 0);
    ASSERT_TRUE(access(parent, F_OK) == 0);

    ASSERT_TRUE(archive_restore_ensure_parent_dir("plain.txt") == 0);
    ASSERT_TRUE(archive_restore_ensure_parent_dir(NULL) != 0);
    ASSERT_TRUE(archive_restore_resolve_dest_root(NULL, "", out, sizeof(out)) != 0);

    char no_meta[PATH_MAX];
    ASSERT_TRUE(join_path_into(no_meta, sizeof(no_meta), root, "no-meta") == 0);
    ASSERT_TRUE(mkdir(no_meta, 0700) == 0);
    ASSERT_TRUE(archive_restore_resolve_dest_root(no_meta, "", out, sizeof(out)) != 0);

    return 0;
}

int main(void)
{
    if (test_archive_paths() != 0)
        return 1;
    if (test_file_ops() != 0)
        return 1;
    if (test_file_ops_failures() != 0)
        return 1;
    if (test_archive_snapshot_status() != 0)
        return 1;
    if (test_archive_version_path() != 0)
        return 1;
    if (test_archive_restore_rules() != 0)
        return 1;
    if (test_archive_restore_dest_dirs() != 0)
        return 1;
    return 0;
}
