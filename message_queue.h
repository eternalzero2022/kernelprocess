#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <linux/ioctl.h>

#define MSGQ_SET_PRIORITY _IOW('q', 1, int) // ioctl command to set the priority of messages, for simplicity we will not implement this command in this example

#endif