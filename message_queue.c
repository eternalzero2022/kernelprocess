#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time64.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include "message_queue.h"

#define DEVICE_NAME "message_queue_simple"
#define MAX_MSG_SIZE 256
#define MAX_MSG_COUNT 10
#define MAX_PRIORITY 10

struct message
{
    struct list_head list;
    int len;
    int priority;
    char data[MAX_MSG_SIZE];
};

struct msgqueue
{
    struct list_head msg_list;
    int msg_count;
    int max_msg_count;
    struct mutex lock;
    wait_queue_head_t not_empty; // reader wait
    wait_queue_head_t not_full;  // writer wait
};

struct filp_private_data
{
    struct msgqueue *queue;
    int current_priority;
};

static void init_msgqueue(struct msgqueue *queue);
static int enqueue_msg(struct msgqueue *queue, const char __user *buf, size_t count, int priority);
static struct message *dequeue_msg(struct msgqueue *queue);

static int msgqueue_open(struct inode *inode, struct file *file);
static int msgqueue_release(struct inode *inode, struct file *file);
static ssize_t msgqueue_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t msgqueue_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);
static long msgqueue_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

// global message queue
static struct msgqueue *global_queue;
static int major_num;
static int device_open_count;

// initialize the message queue
static void init_msgqueue(struct msgqueue *queue)
{
    INIT_LIST_HEAD(&queue->msg_list);
    queue->msg_count = 0;
    queue->max_msg_count = MAX_MSG_COUNT;
    mutex_init(&queue->lock);
    init_waitqueue_head(&queue->not_empty);
    init_waitqueue_head(&queue->not_full);
}

static int enqueue_msg(struct msgqueue *queue, const char __user *buf, size_t count, int priority)
{
    struct message *msg, *pos;
    struct list_head *insert_pos = &queue->msg_list; // default to insert at the end of the list

    if (count > MAX_MSG_SIZE)
    {
        count = MAX_MSG_SIZE;
    }

    // if (queue->msg_count >=  queue->max_msg_count)
    // {
    //     return -ENOSPC;
    // }

    msg = kmalloc(sizeof(*msg), GFP_KERNEL);
    if (!msg)
    { // out of memory
        return -ENOMEM;
    }

    msg->len = count;
    if (copy_from_user(msg->data, buf, count))
    { // number of bytes that could not be copied
        kfree(msg);
        return -EFAULT;
    }

    msg->priority = priority;

    // list_add_tail(&msg->list, &queue->msg_list);
    // insert the message into the queue based on priority, higher priority messages are placed before lower priority messages
    list_for_each_entry(pos, &queue->msg_list, list)
    {
        if (msg->priority < pos->priority)
        {
            insert_pos = &pos->list;
            break;
        }
    }
    list_add_tail(&msg->list, insert_pos);

    queue->msg_count++;

    return count;
}

static struct message *dequeue_msg(struct msgqueue *queue)
{
    printk(KERN_INFO "message_queue: dequeue_msg called, current message count: %d\n", queue->msg_count);
    struct message *msg;
    if (list_empty(&queue->msg_list))
    {
        printk(KERN_INFO "message_queue: dequeue_msg called but queue is empty\n");
        return NULL;
    }

    msg = list_first_entry(&queue->msg_list, struct message, list);
    printk(KERN_INFO "message_queue: dequeueing message of length %d\n", msg->len);
    list_del(&msg->list);
    queue->msg_count--;
    printk(KERN_INFO "message_queue: message dequeued, current message count: %d\n", queue->msg_count);

    return msg;
}

static int msgqueue_open(struct inode *inode, struct file *filp)
{
    struct filp_private_data *private_data;
    private_data = kmalloc(sizeof(*private_data), GFP_KERNEL);
    if (!private_data)
    {
        printk(KERN_ERR "message_queue: failed to allocate memory for private data\n");
        return -ENOMEM;
    }
    private_data->queue = global_queue;
    private_data->current_priority = 5;
    // filp->private_data = global_queue; // store the message queue pointer in private_data, because we only have one message queue, we can directly use the global pointer
    filp->private_data = private_data;
    try_module_get(THIS_MODULE); // increase the module reference count to prevent the module from being removed while it's in use
    device_open_count++;
    printk(KERN_INFO "message_queue: device opened. current device open count: %d\n", device_open_count);
    return 0;
}

static int msgqueue_release(struct inode *inode, struct file *filp)
{
    kfree(filp->private_data); // free the private data allocated in open
    module_put(THIS_MODULE);   // decrease the module reference count
    device_open_count--;
    printk(KERN_INFO "message_queue: device closed. current device open count: %d\n", device_open_count);
    return 0;
}

static ssize_t msgqueue_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    struct filp_private_data *fpd = filp->private_data;
    struct msgqueue *queue = fpd->queue; // get the message queue pointer from private_data
    struct message *msg;
    ssize_t ret;

    if (count == 0)
    {
        return 0;
    }

    // printk(KERN_INFO "message_queue: read requested for %zu bytes\n", count);

    mutex_lock(&queue->lock);
    while (list_empty(&queue->msg_list))
    {
        // printk(KERN_INFO "message_queue: no messages in queue, reader is going to sleep\n");
        mutex_unlock(&queue->lock);
        if (filp->f_flags & O_NONBLOCK)
        {
            // printk(KERN_INFO "message_queue: non-blocking read, returning -EAGAIN\n");
            return -EAGAIN; // non-blocking mode, return immediately
        }
        wait_event_interruptible(queue->not_empty, !list_empty(&queue->msg_list)); // wait until there is a message in the queue. waits only when  list_empty is true
        mutex_lock(&queue->lock);
    }
    // printk(KERN_INFO "message_queue: message available, reader is waking up\n");

    msg = dequeue_msg(queue); // msg is malloced in dequeue_msg, we need to free it after reading

    // printk(KERN_INFO "message_queue: read message: %s\n", msg->data);

    wake_up_interruptible(&queue->not_full);
    mutex_unlock(&queue->lock);

    if (!msg)
    {
        return -EFAULT; // should not happen
    }

    if (count > msg->len)
    {
        count = msg->len;
    }

    // printk(KERN_INFO "message_queue: copying message to user space, message length: %d, bytes to copy: %zu\n", msg->len, count);

    if (copy_to_user(buf, msg->data, count))
    { // number of bytes that could not be copied
        kfree(msg);
        return -EFAULT;
    }

    // printk(KERN_INFO "message_queue: message copied to user space, bytes copied: %zu\n", count);

    ret = count;
    kfree(msg); // free the message after reading

    return ret;
}

static ssize_t msgqueue_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    printk(KERN_INFO "message_queue: write requested for %zu bytes\n", count);
    struct filp_private_data *fpd = filp->private_data;
    struct msgqueue *queue = fpd->queue; // get the message queue pointer from private_data
    ssize_t ret;

    int priority = fpd->current_priority; // use the current priority for messages in the queue

    mutex_lock(&queue->lock);

    // ret = enqueue_msg(queue, buf, count, priority);
    while (queue->msg_count >= queue->max_msg_count)
    {
        mutex_unlock(&queue->lock);
        if (filp->f_flags & O_NONBLOCK)
        {
            // printk(KERN_INFO "message_queue: non-blocking write, returning -EAGAIN\n");
            return -EAGAIN; // non-blocking mode, return immediately
        }
        wait_event_interruptible(queue->not_full, queue->msg_count < queue->max_msg_count);
        mutex_lock(&queue->lock);
    }

    ret = enqueue_msg(queue, buf, count, priority);

    // printk(KERN_INFO "message_queue: write message: %s\n", buf);

    if (ret > 0)
    {
        wake_up_interruptible(&queue->not_empty); // wake up one reader waiting for a message
    }

    mutex_unlock(&queue->lock);

    return ret;
}

static long msgqueue_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    // we can use ioctl to implement some control commands for the message queue, such as clearing the queue, getting the current message count, etc.
    // but for simplicity, we will not implement any ioctl commands in this example
    struct filp_private_data *fpd = filp->private_data;
    int priority;

    switch (cmd)
    {
    case MSGQ_SET_PRIORITY:
        if (copy_from_user(&priority, (int __user *)arg, sizeof(int)))
        {
            return -EFAULT;
        }
        if (priority < 0 || priority > MAX_PRIORITY)
        {
            return -EINVAL; // invalid priority value
        }
        fpd->current_priority = priority; // set the current priority for messages in the queue
        printk(KERN_INFO "message_queue: set current message priority to %d\n", fpd->current_priority);
        return 0;
    default:
        return -EINVAL; // invalid command
    }
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = msgqueue_open,
    .release = msgqueue_release,
    .read = msgqueue_read,
    .write = msgqueue_write,
    .unlocked_ioctl = msgqueue_ioctl,
};

static int __init message_queue_init(void)
{
    global_queue = kmalloc(sizeof(*global_queue), GFP_KERNEL);
    if (!global_queue)
    {
        printk(KERN_ERR "message_queue: failed to allocate memory for message queue\n");
        return -ENOMEM;
    }
    init_msgqueue(global_queue);

    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0)
    {
        printk(KERN_ERR "message_queue: failed to register character device\n");
        kfree(global_queue);
        return major_num;
    }
    device_open_count = 0;

    printk(KERN_INFO "message_queue: module loaded with device major number %d\n", major_num);
    return 0;
}

static void __exit message_queue_exit(void)
{
    struct message *msg, *tmp;
    unregister_chrdev(major_num, DEVICE_NAME);

    // free all messages in the queue
    list_for_each_entry_safe(msg, tmp, &global_queue->msg_list, list)
    {
        list_del(&msg->list);
        kfree(msg);
    }

    kfree(global_queue);
    printk(KERN_INFO "message_queue: module unloaded\n");
}

MODULE_LICENSE("GPL");

module_init(message_queue_init);
module_exit(message_queue_exit);
