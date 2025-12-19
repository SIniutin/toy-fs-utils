cmake_minimum_required(VERSION 3.16)
project(trash_tools C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

target_include_directories(utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(queue PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(crawler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(crawler PUBLIC utils)


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


add_tool(diskusage src/diskusage.c)
add_tool(fswatch   src/fswatch.c)

add_tool(rm_trash   src/trashbin/rm_trash.c)
add_tool(list_trash src/trashbin/list_trash.c)
add_tool(untrash    src/trashbin/untrash.c)

add_tool(backup       src/archive/backup.c)
add_tool(backup_list  src/archive/backup_list.c)
add_tool(upback       src/archive/upback.c)
