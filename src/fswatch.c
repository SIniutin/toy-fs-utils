#include <sys/inotify.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define EVENTS IN_ACCESS | IN_ATTRIB | IN_DELETE | IN_MODIFY | IN_MOVE | IN_OPEN | IN_CLOSE

FILE *log_file;

char *get_current_datetime()
{
    static char buffer[20];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeinfo);
    return buffer;
}

const char *get_event_name(uint32_t mask)
{
    if (mask & IN_ACCESS)
        return "ACCESS";
    if (mask & IN_ATTRIB)
        return "ATTRIB";
    if (mask & IN_CLOSE_WRITE)
        return "CLOSE_WRITE";
    if (mask & IN_CLOSE_NOWRITE)
        return "CLOSE_NOWRITE";
    if (mask & IN_CREATE)
        return "CREATE";
    if (mask & IN_DELETE)
        return "DELETE";
    if (mask & IN_DELETE_SELF)
        return "DELETE_SELF";
    if (mask & IN_MODIFY)
        return "MODIFY";
    if (mask & IN_MOVE_SELF)
        return "MOVE_SELF";
    if (mask & IN_MOVED_FROM)
        return "MOVED_FROM";
    if (mask & IN_MOVED_TO)
        return "MOVED_TO";
    if (mask & IN_OPEN)
        return "OPEN";
    return "UNKNOWN";
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3)
    {
        puts("usage: ./fswatch <dir-name> <log-name>\n");
        return 0;
    }
    int fd = inotify_init1(IN_CLOEXEC);
    if (fd < 0)
    {
        fputs("fswatch: failed to initialize inotify\n", stderr);
        return 1;
    }
    if (argc == 3)
    {
        log_file = fopen(argv[2], "w");
    }
    else
        log_file = stdout;
    int wd = inotify_add_watch(fd, argv[1], (uint32_t)EVENTS);
    if (wd == -1)
    {
        fprintf(stderr, "fswatch: couldn't add watch to %s\n", argv[1]);
        return 2;
    }
    while (1)
    {
        char buffer[256 * (sizeof(struct inotify_event) + 16)];
        int length = read(fd, buffer, 256 * (sizeof(struct inotify_event) + 16));
        if (length < 0)
        {
            fputs("fswatch: failed to read inotify\n", stderr);
            return 2;
        }
        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", argv[1], event->name);
            const char *event_name = get_event_name(event->mask);
            fprintf(log_file, "[%s], %s, %s\n", get_current_datetime(), path, event_name);
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    return 0;
}
