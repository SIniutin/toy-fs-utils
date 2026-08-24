#define _XOPEN_SOURCE 700

#define main fswatch_program_main
#include "../src/fswatch.c"
#undef main

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

int main(void)
{
    ASSERT_TRUE(strcmp(get_event_name(IN_ACCESS), "ACCESS") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_ATTRIB), "ATTRIB") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_CLOSE_WRITE), "CLOSE_WRITE") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_CLOSE_NOWRITE), "CLOSE_NOWRITE") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_CREATE), "CREATE") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_DELETE), "DELETE") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_DELETE_SELF), "DELETE_SELF") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_MODIFY), "MODIFY") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_MOVE_SELF), "MOVE_SELF") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_MOVED_FROM), "MOVED_FROM") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_MOVED_TO), "MOVED_TO") == 0);
    ASSERT_TRUE(strcmp(get_event_name(IN_OPEN), "OPEN") == 0);
    ASSERT_TRUE(strcmp(get_event_name(0), "UNKNOWN") == 0);

    char *ts = get_current_datetime();
    ASSERT_TRUE(ts != NULL);
    ASSERT_TRUE(strlen(ts) == 16);
    return 0;
}
