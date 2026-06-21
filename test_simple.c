// test_simple.c - 测试简单版本（FIFO，单写单读）
#include "test_common.h"

#define NUM_WRITERS 1
#define NUM_READERS 1

void writer_process(int id)
{
    char buf[64];

    for (int i = 0; i < NUM_MESSAGES; i++)
    {
        int fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
        {
            perror("writer open");
            exit(1);
        }

        snprintf(buf, sizeof(buf), "W%d:Msg%02d", id, i);
        ssize_t ret = write(fd, buf, strlen(buf));

        if (ret > 0)
        {
            printf(COLOR_GREEN "[WRITER %d] Wrote: %s (len=%zd)%s\n",
                   id, buf, ret, COLOR_RESET);
        }
        else
        {
            printf(COLOR_RED "[WRITER %d] Write failed: %s%s\n",
                   id, strerror(errno), COLOR_RESET);
        }

        close(fd);
        // usleep(50000); // 50ms 延迟，让输出更可读
    }
}

void reader_process(int id)
{
    char buf[MAX_MSG_SIZE];

    for (int i = 0; i < NUM_MESSAGES; i++)
    {
        int fd = open(DEVICE_PATH, O_RDONLY);
        if (fd < 0)
        {
            perror("reader open");
            exit(1);
        }

        ssize_t ret = read(fd, buf, sizeof(buf) - 1);

        if (ret > 0)
        {
            buf[ret] = '\0';
            printf(COLOR_CYAN "[READER %d] Read: %s (len=%zd)%s\n",
                   id, buf, ret, COLOR_RESET);
        }
        else if (ret == 0)
        {
            printf(COLOR_YELLOW "[READER %d] No message (EOF)%s\n",
                   id, COLOR_RESET);
        }
        else
        {
            printf(COLOR_RED "[READER %d] Read failed: %s%s\n",
                   id, strerror(errno), COLOR_RESET);
        }

        close(fd);
        // usleep(50000);
    }
}

int main()
{
    if (!check_device())
        return 1;

    printf(COLOR_MAGENTA "\n========== Simple Version Test ==========\n" COLOR_RESET);
    printf("Test: Single writer, single reader, FIFO order\n");
    printf("Each process will read/write %d messages\n\n", NUM_MESSAGES);

    // 先清空可能存在的残留消息
    printf("Clearing existing messages...\n");
    for (int i = 0; i < NUM_MESSAGES * 2; i++)
    {
        int fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
        if (fd >= 0)
        {
            char buf[256];
            read(fd, buf, sizeof(buf));
            close(fd);
        }
    }

    pid_t writer_pid, reader_pid;

    writer_pid = fork();
    if (writer_pid == 0)
    {
        writer_process(0);
        exit(0);
    }

    reader_pid = fork();
    if (reader_pid == 0)
    {
        reader_process(0);
        exit(0);
    }

    // 等待两个进程完成
    waitpid(writer_pid, NULL, 0);
    waitpid(reader_pid, NULL, 0);

    printf(COLOR_MAGENTA "\n========== Simple Version Test Complete ==========\n" COLOR_RESET);

    return 0;
}