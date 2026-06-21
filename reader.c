// reader.c - 支持多进程并发读取
#include "test_common.h"
#include <sys/types.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int delay = (argc > 1) ? atoi(argv[1]) : 0;
    int nonblock = (argc > 2 && strcmp(argv[2], "nonblock") == 0);

    pid_t pid = getpid();

    // 打开设备
    int flags = O_RDONLY;
    if (nonblock)
    {
        flags |= O_NONBLOCK;
    }

    int fd = open(DEVICE_PATH, flags);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    char buf[MAX_MSG_SIZE];
    ssize_t ret = read(fd, buf, sizeof(buf) - 1);

    if (ret < 0)
    {
        if (errno == EAGAIN)
        {
            printf(COLOR_YELLOW "[READER] PID=%d: No message (non-blocking)\n" COLOR_RESET, pid);
        }
        else
        {
            perror("read");
        }
    }
    else if (ret == 0)
    {
        printf(COLOR_YELLOW "[READER] PID=%d: No message (EOF)\n" COLOR_RESET, pid);
    }
    else
    {
        buf[ret] = '\0';
        printf(COLOR_CYAN "[READER] PID=%d, Read %zd bytes: \"%s\"%s\n",
               pid, ret, buf, COLOR_RESET);
    }

    if (delay > 0)
    {
        usleep(delay * 1000);
    }

    close(fd);
    return 0;
}