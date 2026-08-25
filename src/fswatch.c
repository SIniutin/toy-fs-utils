#include <sys/inotify.h>

#include <stdlib.h>
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
        if (log_file && log_file != stdout)
            fclose(log_file);
        close(fd);
        return 2;
    }

    long max_events = -1;
    const char *max_events_env = getenv("FSWATCH_MAX_EVENTS");
    if (max_events_env)
    {
        char *end = NULL;
        max_events = strtol(max_events_env, &end, 10);
        if (end == max_events_env || *end != '\0' || max_events < 0)
            max_events = -1;
    }

    long seen_events = 0;
    while (1)
    {
        char buffer[256 * (sizeof(struct inotify_event) + 16)];
        ssize_t length = read(fd, buffer, 256 * (sizeof(struct inotify_event) + 16));
        if (length < 0)
        {
            fputs("fswatch: failed to read inotify\n", stderr);
            if (log_file && log_file != stdout)
                fclose(log_file);
            inotify_rm_watch(fd, wd);
            close(fd);
            return 2;
        }
        ssize_t i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", argv[1], event->name);
            const char *event_name = get_event_name(event->mask);
            fprintf(log_file, "[%s], %s, %s\n", get_current_datetime(), path, event_name);
            fflush(log_file);
            seen_events++;
            if (max_events >= 0 && seen_events >= max_events)
                goto done;
            i += sizeof(struct inotify_event) + event->len;
        }
    }

done:
    inotify_rm_watch(fd, wd);
    if (log_file && log_file != stdout)
        fclose(log_file);
    close(fd);
    return 0;
}
