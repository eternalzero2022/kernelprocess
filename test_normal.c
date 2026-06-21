// test_normal.c - 测试中级版本（优先级，单写单读）
#include "test_common.h"

#define NUM_WRITERS 1
#define NUM_READERS 1

// 定义测试消息及其优先级
typedef struct
{
    int priority;
    const char *message;
} TestMessage;

TestMessage test_messages[] = {
    {5, "Low priority message"},
    {1, "HIGH priority message"},
    {8, "Very low priority message"},
    {3, "Medium-high priority"},
    {0, "CRITICAL priority"},
    {6, "Medium-low priority"},
    {2, "High priority message"},
    {9, "Lowest priority"},
    {4, "Medium priority"},
    {7, "Low priority"},
};

#define NUM_TEST_MSGS (sizeof(test_messages) / sizeof(test_messages[0]))

void writer_process(int id)
{
    for (int i = 0; i < NUM_MESSAGES; i++)
    {
        int fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
        {
            perror("writer open");
            exit(1);
        }

        // 循环使用测试消息
        int idx = i % NUM_TEST_MSGS;
        int priority = test_messages[idx].priority;
        const char *msg = test_messages[idx].message;

        // 设置优先级
        if (ioctl(fd, MSGQ_SET_PRIORITY, &priority) < 0)
        {
            perror("ioctl SET_PRIORITY");
            close(fd);
            exit(1);
        }

        ssize_t ret = write(fd, msg, strlen(msg));

        if (ret > 0)
        {
            printf(COLOR_GREEN "[WRITER %d] Priority=%d, Wrote: %s (len=%zd)%s\n",
                   id, priority, msg, ret, COLOR_RESET);
        }
        else
        {
            printf(COLOR_RED "[WRITER %d] Write failed: %s%s\n",
                   id, strerror(errno), COLOR_RESET);
        }

        close(fd);
        usleep(100000); // 100ms 延迟
    }
}

void reader_process(int id)
{
    char buf[MAX_MSG_SIZE];
    int last_priority = -1;
    int is_decreasing = 1;

    printf(COLOR_YELLOW "\n[READER] Reading %d messages (should be in priority order)...\n"
                        "Expected: priority 0,1,2,3,4,5,6,7,8,9 (or better: strictly decreasing priority number)\n\n" COLOR_RESET,
           NUM_MESSAGES);

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

            // 尝试从消息中判断优先级（通过消息内容）
            // 由于消息不包含优先级，我们从 ioctl 无法获取
            // 但可以通过观察顺序来判断

            printf(COLOR_CYAN "[READER %d] Read (%d/%d): %s%s\n",
                   id, i + 1, NUM_MESSAGES, buf, COLOR_RESET);
        }
        else if (ret == 0)
        {
            printf(COLOR_YELLOW "[READER %d] No more messages%s\n",
                   id, COLOR_RESET);
            break;
        }
        else
        {
            printf(COLOR_RED "[READER %d] Read failed: %s%s\n",
                   id, strerror(errno), COLOR_RESET);
        }

        close(fd);
        usleep(50000);
    }
}

// 独立的优先级顺序验证程序
void verify_priority_order()
{
    printf(COLOR_MAGENTA "\n=== Priority Order Verification ===\n" COLOR_RESET);

    // 清空队列
    printf("Clearing queue...\n");
    for (int i = 0; i < 20; i++)
    {
        int fd = open(DEVICE_PATH, O_RDONLY | O_NONBLOCK);
        if (fd >= 0)
        {
            char buf[256];
            read(fd, buf, sizeof(buf));
            close(fd);
        }
    }

    // 按乱序写入不同优先级的消息
    printf("\nWriting messages in random priority order:\n");

    struct
    {
        int priority;
        const char *msg;
    } test_data[] = {
        {5, "5: Medium"},
        {1, "1: High"},
        {8, "8: Very Low"},
        {3, "3: Medium-High"},
        {0, "0: CRITICAL"},
        {6, "6: Medium-Low"},
        {2, "2: High+1"},
        {9, "9: Lowest"},
        {4, "4: Medium"},
        {7, "7: Low"},
    };

    for (int i = 0; i < 10; i++)
    {
        int fd = open(DEVICE_PATH, O_WRONLY);
        if (fd < 0)
        {
            perror("open");
            return;
        }

        int priority = test_data[i].priority;
        if (ioctl(fd, MSGQ_SET_PRIORITY, &priority) < 0)
        {
            perror("ioctl");
            close(fd);
            return;
        }

        ssize_t ret = write(fd, test_data[i].msg, strlen(test_data[i].msg));
        if (ret > 0)
        {
            printf("  Wrote: priority=%d, msg=\"%s\"\n", priority, test_data[i].msg);
        }

        close(fd);
    }

    printf("\nReading messages (should be in priority order 0,1,2,3,4,5,6,7,8,9):\n");

    int prev_priority = -1;
    for (int i = 0; i < 10; i++)
    {
        int fd = open(DEVICE_PATH, O_RDONLY);
        if (fd < 0)
            break;

        char buf[256];
        ssize_t ret = read(fd, buf, sizeof(buf) - 1);
        if (ret > 0)
        {
            buf[ret] = '\0';
            printf("  Read %d/%d: %s\n", i + 1, 10, buf);

            // 从消息内容提取优先级进行验证
            int priority = atoi(buf);
            if (prev_priority >= 0 && priority < prev_priority)
            {
                // 优先级数字越小越好，所以应该递增
                // 实际上我们期望 0,1,2,3,4,5,6,7,8,9
                if (priority > prev_priority)
                {
                    // 正确顺序
                }
                else
                {
                    printf(COLOR_RED "    Warning: Priority order might be incorrect!%s\n", COLOR_RESET);
                }
            }
            prev_priority = priority;
        }
        close(fd);
    }

    printf(COLOR_MAGENTA "=== Priority Verification Complete ===\n" COLOR_RESET);
}

int main()
{
    if (!check_device())
        return 1;

    printf(COLOR_MAGENTA "\n========== Normal Version Test ==========\n" COLOR_RESET);
    printf("Test: Priority-based message queue, single writer, single reader\n");
    printf("Each process will read/write %d messages\n\n", NUM_MESSAGES);

    // 先进行优先级顺序验证
    verify_priority_order();

    printf(COLOR_MAGENTA "\n========== Normal Version Test Complete ==========\n" COLOR_RESET);

    return 0;
}