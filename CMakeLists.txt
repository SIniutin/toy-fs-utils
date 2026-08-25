cmake_minimum_required(VERSION 3.16)
project(trash_tools C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

option(BUILD_BENCHMARKS "Build benchmark binaries" OFF)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(utils STATIC
    src/utils/common.c
)

add_library(queue STATIC
    src/utils/queue.c
)

add_library(crawler STATIC
    src/utils/crawler.c
)

add_library(diskusage_stats STATIC
    src/diskusage/stats.c
)

add_library(trash_restore STATIC
    src/trashbin/restore.c
)

add_library(trash_paths STATIC
    src/trashbin/paths.c
)

add_library(trash_log STATIC
    src/trashbin/log.c
)

add_library(trash_listing STATIC
    src/trashbin/listing.c
)

add_library(archive_paths STATIC
    src/archive/paths.c
)

add_library(archive_file_ops STATIC
    src/archive/file_ops.c
)

add_library(archive_snapshot STATIC
    src/archive/snapshot.c
)

add_library(archive_restore STATIC
    src/archive/restore.c
)

foreach(lib
    utils
    queue
    crawler
    diskusage_stats
    trash_restore
    trash_paths
    trash_log
    trash_listing
    archive_paths
    archive_file_ops
    archive_snapshot
    archive_restore
)
    target_include_directories(${lib} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endforeach()

target_link_libraries(crawler PUBLIC utils)
target_link_libraries(trash_restore PUBLIC utils)
target_link_libraries(trash_paths PUBLIC utils)
target_link_libraries(trash_log PUBLIC utils)
target_link_libraries(trash_listing PUBLIC trash_log utils)
target_link_libraries(archive_paths PUBLIC utils)
target_link_libraries(archive_file_ops PUBLIC utils archive_paths)
target_link_libraries(archive_snapshot PUBLIC utils archive_paths)
target_link_libraries(archive_restore PUBLIC utils archive_file_ops)


function(add_tool name)
    add_executable(${name} ${ARGN})

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(${name} PRIVATE
        utils
        crawler
        queue
    )
endfunction()


add_tool(diskusage src/diskusage/diskusage.c)
target_link_libraries(diskusage PRIVATE diskusage_stats)
add_tool(fswatch   src/fswatch.c)

add_tool(rm_trash   src/trashbin/rm_trash.c)
target_link_libraries(rm_trash PRIVATE trash_paths)
add_tool(list_trash src/trashbin/list_trash.c)
target_link_libraries(list_trash PRIVATE trash_paths trash_log trash_listing)
add_tool(untrash    src/trashbin/untrash.c)
target_link_libraries(untrash PRIVATE trash_restore trash_paths trash_log)

add_tool(backup       src/archive/backup.c)
target_link_libraries(backup PRIVATE archive_paths archive_file_ops archive_snapshot)
add_tool(backup_list  src/archive/backup_list.c)
target_link_libraries(backup_list PRIVATE archive_paths)
add_tool(upback       src/archive/upback.c)
target_link_libraries(upback PRIVATE archive_paths archive_file_ops archive_restore)

add_executable(test_queue tests/test_queue.c)
target_include_directories(test_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_queue PRIVATE queue)
add_test(NAME queue COMMAND test_queue)

add_executable(test_common tests/test_common.c)
target_include_directories(test_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_common PRIVATE utils)
add_test(NAME common COMMAND test_common)

add_executable(test_crawler tests/test_crawler.c)
target_include_directories(test_crawler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_crawler PRIVATE crawler queue utils)
add_test(NAME crawler COMMAND test_crawler)

add_executable(diskusage_unit tests/test_diskusage_unit.c)
target_include_directories(diskusage_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(diskusage_unit PRIVATE diskusage_stats)
add_test(NAME diskusage_unit COMMAND diskusage_unit)

add_executable(trash_restore_unit tests/test_trash_restore.c)
target_include_directories(trash_restore_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(trash_restore_unit PRIVATE utils trash_restore)
add_test(NAME trash_restore_unit COMMAND trash_restore_unit)

add_executable(trash_log_unit tests/test_trash_log.c)
target_include_directories(trash_log_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(trash_log_unit PRIVATE trash_log utils)
add_test(NAME trash_log_unit COMMAND trash_log_unit)

add_executable(trash_listing_unit tests/test_trash_listing.c)
target_include_directories(trash_listing_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(trash_listing_unit PRIVATE trash_listing trash_log utils)
add_test(NAME trash_listing_unit COMMAND trash_listing_unit)

add_executable(archive_unit tests/test_archive.c)
target_include_directories(archive_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(archive_unit PRIVATE utils archive_paths archive_file_ops archive_snapshot archive_restore)
add_test(NAME archive_unit COMMAND archive_unit)

add_executable(fswatch_unit tests/test_fswatch_unit.c)
target_include_directories(fswatch_unit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME fswatch_unit COMMAND fswatch_unit)

add_test(
    NAME trash_e2e
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_trash.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_test(
    NAME diskusage_e2e
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_diskusage.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_test(
    NAME backup_e2e
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backup.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_test(
    NAME fswatch_e2e
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fswatch.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if(BUILD_BENCHMARKS)
    add_executable(bench_crawler bench/bench_crawler.c)
    target_include_directories(bench_crawler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(bench_crawler PRIVATE crawler queue utils)
endif()
