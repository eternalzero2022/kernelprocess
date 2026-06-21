// writer.c - 支持多进程并发写入
#include "test_common.h"
#include <sys/types.h>
#include <errno.h>

void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <priority> <message> [delay_ms]\n", prog);
    fprintf(stderr, "  priority: 0-10 (0=highest, 10=lowest)\n");
    fprintf(stderr, "  message: text to write\n");
    fprintf(stderr, "  delay_ms: optional delay after write (default 0)\n");
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    int priority = atoi(argv[1]);
    char *msg = argv[2];
    int delay = (argc > 3) ? atoi(argv[3]) : 0;

    // 获取进程ID用于标识
    pid_t pid = getpid();

    // 打开设备
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    // 设置优先级
    if (ioctl(fd, MSGQ_SET_PRIORITY, &priority) < 0)
    {
        perror("ioctl SET_PRIORITY");
        close(fd);
        return 1;
    }

    // 写入消息
    int len = strlen(msg);
    if (len > MAX_MSG_SIZE - 1)
    {
        len = MAX_MSG_SIZE - 1;
    }

    ssize_t ret = write(fd, msg, len);
    if (ret < 0)
    {
        perror("write");
        close(fd);
        return 1;
    }

    // 彩色输出
    printf(COLOR_GREEN "[WRITER] PID=%d, Priority=%d, Wrote %zd bytes: \"%s\"%s\n",
           pid, priority, ret, msg, COLOR_RESET);

    if (delay > 0)
    {
        usleep(delay * 1000);
    }

    close(fd);
    return 0;
}