// test_common.h
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define DEVICE_PATH "/dev/queue"
#define MAX_MSG_SIZE 256
#define NUM_MESSAGES 20

// ioctl 命令（中级和高级版本需要）
#define MSGQUEUE_MAGIC 'q'
#define MSGQ_SET_PRIORITY _IOW(MSGQUEUE_MAGIC, 1, int)

// 颜色输出
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_RESET "\033[0m"

// 检查设备是否存在
static inline int check_device(void)
{
    if (access(DEVICE_PATH, F_OK) != 0)
    {
        fprintf(stderr, "Device %s not found. Please load the kernel module first.\n", DEVICE_PATH);
        return 0;
    }
    return 1;
}

#endif