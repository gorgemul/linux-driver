#include <linux/init.h> // THIS_MODULE
#include <linux/module.h> // module_init, module_exit
#include <linux/mutex.h> // struct mutex
#include <linux/cdev.h> // struct cdev, cdev_add, cdev_del
#include <linux/errno.h> // ENOMEM, ERESTARTSYS, EAGAIN, EFAULT
#include <linux/fs.h> // alloc_chrdev_region, unregister_chrdev_region, file_operations, struct inode, struct file
#include <linux/slab.h> // kmalloc, kfree
#include <linux/string.h> // memset
#include <linux/wait.h> // wait_queue_head_t
#include <linux/container_of.h> // container_of
#include <uapi/asm-generic/fcntl.h> // O_NONBLOCK
#include <linux/uaccess.h> // copy_to_user, copy_from_user
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("blocking in memory devices");

#define NUM_DEVICES        2
#define FIRST_MINOR        0
#define DEFAULT_BUFFER_LEN 6

unsigned int major;

struct scull_block_dev {
        wait_queue_head_t read_queue;
        wait_queue_head_t write_queue;
        char *buffer;
        char *read_ptr;
        char *write_ptr;
        size_t buffer_len;
        int num_readers;
        int num_writers;
        struct mutex lock;
        struct cdev cdev;
};

// caller should hold a lock
static size_t left_bytes_to_write(struct scull_block_dev *dev)
{
        if (dev->read_ptr == dev->write_ptr)
                return dev->buffer_len - 1;
        return ((dev->read_ptr + dev->buffer_len - dev->write_ptr) % dev->buffer_len) - 1;
}

static int scull_block_open(struct inode *inode, struct file *filp)
{
        struct scull_block_dev *dev = container_of(inode->i_cdev, struct scull_block_dev, cdev);
        filp->private_data = dev;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        if (dev->buffer == NULL) {
                dev->buffer = kmalloc(dev->buffer_len, GFP_KERNEL);
                if (dev->buffer == NULL) {
                        mutex_unlock(&dev->lock);
                        printk(KERN_ERR "scull_block_open kmalloc fail");
                        return -ENOMEM;
                }
                dev->read_ptr = dev->write_ptr = dev->buffer;
        }
        if (filp->f_mode & FMODE_READ)
                dev->num_readers++;
        if (filp->f_mode & FMODE_WRITE)
                dev->num_writers++;
        mutex_unlock(&dev->lock);
        return nonseekable_open(inode, filp);
}

static int scull_block_release(struct inode *inode, struct file *filp)
{
        struct scull_block_dev *dev = filp->private_data;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        if (filp->f_mode & FMODE_READ)
                dev->num_readers--;
        if (filp->f_mode & FMODE_WRITE)
                dev->num_writers--;
        if (dev->num_writers == 0 && dev->num_readers == 0) {
                kfree(dev->buffer);
                dev->buffer = NULL;
                dev->read_ptr = dev->write_ptr = 0;
        }
        mutex_unlock(&dev->lock);
        return 0;
}

static ssize_t scull_block_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
        struct scull_block_dev *dev = filp->private_data;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        while (dev->read_ptr == dev->write_ptr) {
                mutex_unlock(&dev->lock);
                if (filp->f_flags & O_NONBLOCK)
                        return -EAGAIN;
                if (wait_event_interruptible(dev->read_queue, (dev->read_ptr != dev->write_ptr)))
                        return -ERESTARTSYS;
                if (mutex_lock_interruptible(&dev->lock))
                        return -ERESTARTSYS;
        }
        char *end = dev->buffer + dev->buffer_len;
        if (dev->write_ptr > dev->read_ptr)
                count = min(count, (size_t)(dev->write_ptr - dev->read_ptr));
        else
                count = min(count, (size_t)(end - dev->read_ptr));
        if (copy_to_user(buf, dev->read_ptr, count)) {
                mutex_unlock(&dev->lock);
                return -EFAULT;
        }
        dev->read_ptr += count;
        if (dev->read_ptr == end)
                dev->read_ptr = dev->buffer;
        mutex_unlock(&dev->lock);
        wake_up_interruptible(&dev->write_queue);
        return count;
}

static ssize_t scull_block_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
        struct scull_block_dev *dev = filp->private_data;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        while (left_bytes_to_write(dev) == 0) {
                mutex_unlock(&dev->lock);
                if (filp->f_flags & O_NONBLOCK)
                        return -EAGAIN;
                if (wait_event_interruptible(dev->write_queue, (left_bytes_to_write(dev) > 0)))
                        return -ERESTARTSYS;
                if (mutex_lock_interruptible(&dev->lock))
                        return -ERESTARTSYS;
        }
        char *end = dev->buffer + dev->buffer_len;
        count = min(count, left_bytes_to_write(dev));
        if (dev->write_ptr >= dev->read_ptr)
                count = min(count, (size_t)(end - dev->write_ptr));
        else
                count = min(count, (size_t)(dev->read_ptr - dev->write_ptr - 1));
        if (copy_from_user(dev->write_ptr, buf, count)) {
                mutex_unlock(&dev->lock);
                return -EFAULT;
        }
        dev->write_ptr += count;
        if (dev->write_ptr == end)
                dev->write_ptr = dev->buffer;
        mutex_unlock(&dev->lock);
        wake_up_interruptible(&dev->read_queue);
        return count;
}


struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = scull_block_open,
        .release = scull_block_release,
        .read = scull_block_read,
        .write = scull_block_write,
};

struct scull_block_dev *scull_block_devices = NULL;

static void scull_block_cleanup(void)
{
        if (scull_block_devices == NULL)
                return;
        for (int i = 0; i < NUM_DEVICES; i++) {
                if (scull_block_devices[i].buffer)
                        kfree(scull_block_devices[i].buffer);
                if (scull_block_devices[i].cdev.owner)
                        cdev_del(&scull_block_devices[i].cdev);
        }
        kfree(scull_block_devices);
        unregister_chrdev_region(MKDEV(major, FIRST_MINOR), NUM_DEVICES);
}

static void __exit scull_block_exit(void)
{
        scull_block_cleanup();
}

static void scull_block_cdev_init(struct cdev *cdev, dev_t dev_num)
{
        int rc;

        cdev_init(cdev, &fops);
        cdev->owner = THIS_MODULE;
        rc = cdev_add(cdev, dev_num, 1);
        if (rc < 0) {
                cdev->owner = NULL;
                printk(KERN_ERR "scull_block_cdev_init cdev_add fail\n");
        }
}

static int __init scull_block_init(void)
{
        int rc;
        dev_t dev_num;

        rc = alloc_chrdev_region(&dev_num, FIRST_MINOR, NUM_DEVICES, "scull-block");
        if (rc < 0) {
                printk(KERN_ERR "scull_block_init: alloc_chrdev_region fail\n");
                return rc;
        }
        major = MAJOR(dev_num);
        scull_block_devices = kmalloc(sizeof(*scull_block_devices) * NUM_DEVICES, GFP_KERNEL);
        if (scull_block_devices == NULL) {
                rc = -ENOMEM;
                goto clean;
        }
        memset(scull_block_devices, 0, sizeof(*scull_block_devices) * NUM_DEVICES);
        for (int i = 0; i < NUM_DEVICES; i++) {
                scull_block_devices[i].buffer_len = DEFAULT_BUFFER_LEN;
                init_waitqueue_head(&scull_block_devices[i].read_queue);
                init_waitqueue_head(&scull_block_devices[i].write_queue);
                mutex_init(&scull_block_devices[i].lock);
                scull_block_cdev_init(&scull_block_devices[i].cdev, MKDEV(major, FIRST_MINOR+i));
        }
        return 0;
clean:
        scull_block_cleanup();
        return rc;
}

module_init(scull_block_init);
module_exit(scull_block_exit);
