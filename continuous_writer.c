// continuous_writer.c - 写入指定数量的消息
#include "test_common.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <num_messages> [writer_id]\n", argv[0]);
        return 1;
    }

    int num_messages = atoi(argv[1]);
    int writer_id = (argc > 2) ? atoi(argv[2]) : getpid() % 1000;

    char buf[64];

    printf(COLOR_GREEN "[WRITER %d] Started, will write %d messages%s\n",
           writer_id, num_messages, COLOR_RESET);

    for (int i = 0; i < num_messages; i++)
    {
        int fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
        {
            perror("open");
            printf(COLOR_RED "[WRITER %d] Failed to open device at message %d%s\n",
                   writer_id, i, COLOR_RESET);
            return 1;
        }

        // 随机优先级 0-9
        int priority = rand() % 10;

        if (ioctl(fd, MSGQ_SET_PRIORITY, &priority) < 0)
        {
            perror("ioctl");
            close(fd);
            return 1;
        }

        snprintf(buf, sizeof(buf), "W%d:M%03d", writer_id, i);
        ssize_t ret = write(fd, buf, strlen(buf));

        if (ret > 0)
        {
            printf(COLOR_GREEN "[WRITER %d] [%d/%d] Pri=%d, Wrote: %s%s\n",
                   writer_id, i + 1, num_messages, priority, buf, COLOR_RESET);
        }
        else
        {
            printf(COLOR_RED "[WRITER %d] Write failed at message %d: %s%s\n",
                   writer_id, i, strerror(errno), COLOR_RESET);
        }

        close(fd);

        // 随机延迟 10-50ms，让并发更真实
        // usleep((rand() % 40 + 10) * 1000);
    }

    printf(COLOR_YELLOW "[WRITER %d] Finished. Wrote %d messages.%s\n",
           writer_id, num_messages, COLOR_RESET);

    return 0;
}