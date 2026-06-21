// continuous_reader.c - 持续读取指定时间
#include "test_common.h"
#include <sys/time.h>
#include <signal.h>

int running = 1;

void timeout_handler(int sig)
{
    running = 0;
}

int main(int argc, char *argv[])
{
    int duration = (argc > 1) ? atoi(argv[1]) : 10; // 默认运行10秒
    int reader_id = (argc > 2) ? atoi(argv[2]) : getpid() % 1000;

    char buf[MAX_MSG_SIZE];
    int count = 0;
    struct timeval start, now;

    // 设置超时信号
    signal(SIGALRM, timeout_handler);
    alarm(duration);

    gettimeofday(&start, NULL);

    printf(COLOR_CYAN "[READER %d] Started, will run for %d seconds%s\n",
           reader_id, duration, COLOR_RESET);

    while (running)
    {
        int fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
        {
            if (errno != EAGAIN && errno != ENOENT)
            {
                perror("open");
            }
            usleep(10000);
            continue;
        }

        ssize_t ret = read(fd, buf, sizeof(buf) - 1);

        if (ret > 0)
        {
            buf[ret] = '\0';
            count++;
            printf(COLOR_CYAN "[READER %d] [#%d] %s%s\n",
                   reader_id, count, buf, COLOR_RESET);
        }
        else if (ret == 0)
        {
            // 队列空，稍等
            usleep(50000);
        }
        else if (errno != EAGAIN)
        {
            // 忽略 EAGAIN
        }

        close(fd);

        // 每秒打印一次进度（可选）
        gettimeofday(&now, NULL);
        if (now.tv_sec - start.tv_sec >= 1 && count % 10 == 0)
        {
            // 静默运行，不打印太多
        }
    }

    printf(COLOR_YELLOW "[READER %d] Finished. Read %d messages in %d seconds.%s\n",
           reader_id, count, duration, COLOR_RESET);

    return 0;
}