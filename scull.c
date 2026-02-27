#include "scull.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h> // dev_t
#include <linux/kdev_t.h> // dev_t, MAJOR, MINOR, MKDEV
#include <linux/cdev.h> // struct cdev
#include <linux/string.h> // memset
#include <linux/slab.h> // kmalloc
#include <linux/errno.h> // errcode
#include <linux/uaccess.h> // copy_to_user, copy_from_user
#include <linux/fs.h>
#include <linux/mutex.h>
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("scull impl");

struct quantum_arr {
        void **data;
        struct quantum_arr *next;
};

struct scull_dev {
        struct quantum_arr *data;
        size_t quantum_size;
        unsigned int num_quantum;
        size_t size;
        struct cdev cdev;
        struct mutex lock;
};

unsigned int major;
unsigned int minor = 0;
unsigned int num_devices = 3; 
size_t quantum_size = DEFAULT_QUANTUM_SIZE;
unsigned int num_quantum = DEFAULT_NUM_QUANTUM;
struct scull_dev *scull_devices;

static struct quantum_arr *scull_find_node(struct scull_dev *dev, int index)
{
        if (!dev)
                return NULL;
        struct quantum_arr *qa = dev->data;
        if (qa == NULL) {
                qa = kmalloc(sizeof(*qa), GFP_KERNEL);
                if (qa == NULL)
                        return NULL;
                memset(qa, 0, sizeof(*qa));
                dev->data = qa;
        }
        while (index--) {
                if (qa->next == NULL) {
                        qa->next = kmalloc(sizeof(*qa->next), GFP_KERNEL);
                        if (qa->next == NULL)
                                return NULL;
                        memset(qa->next, 0, sizeof(*qa->next));
                }
                qa = qa->next;
        }
        return qa;
}

static int scull_trim(struct scull_dev *dev)
{
        if (!dev)
                return 0;
        struct quantum_arr *cur, *next;
        for (cur = dev->data; cur != NULL; cur = next) {
                if (cur->data) {
                        for (int i = 0; i < dev->num_quantum; i++)
                                kfree(cur->data[i]);
                }
                kfree(cur->data);
                next = cur->next;
                kfree(cur);
        }
        dev->data = NULL;
        dev->quantum_size = quantum_size;
        dev->num_quantum = num_quantum;
        dev->size = 0;
        return 0;
}

static int scull_open(struct inode *inode, struct file *filp)
{
        struct scull_dev *dev = container_of(inode->i_cdev, struct scull_dev, cdev);
        filp->private_data = dev;
        if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
                if (mutex_lock_interruptible(&dev->lock))
                        return -ERESTARTSYS;
                scull_trim(dev);
                mutex_unlock(&dev->lock);
        }
        return 0;
}

// since scull is only a in-memory device, do not have any physical device to shutdown
static int scull_release(struct inode *inode, struct file *filp)
{
        return 0;
}

static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
        ssize_t rc = 0;
        struct scull_dev *dev = filp->private_data;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        if (*fpos >= dev->size)
                goto out;
        if (*fpos + count > dev->size)
                count = dev->size - *fpos;
        size_t node_size = dev->quantum_size * dev->num_quantum;
        int node_index = *fpos / node_size;
        int node_offset = *fpos % node_size;
        int quantum_index = node_offset / dev->quantum_size;
        int quantum_offset = node_offset % dev->quantum_size;
        struct quantum_arr *qa = scull_find_node(dev, node_index);
        if (qa == NULL || qa->data == NULL || qa->data[quantum_index] == NULL)
                goto out;
        if (quantum_offset + count > dev->quantum_size)
                count = dev->quantum_size - quantum_offset;
        if (copy_to_user(buf, qa->data[quantum_index] + quantum_offset, count)) {
                rc = -EFAULT;
                goto out;
        }

        *fpos += count;
        rc = count;
out:
        mutex_unlock(&dev->lock);
        return rc;
}

static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
        ssize_t rc = -ENOMEM;
        struct scull_dev *dev = filp->private_data;
        if (mutex_lock_interruptible(&dev->lock))
                return -ERESTARTSYS;
        size_t node_size = dev->quantum_size * dev->num_quantum;
        int node_index = *fpos / node_size;
        int node_offset = *fpos % node_size;
        int quantum_index = node_offset / dev->quantum_size;
        int quantum_offset = node_offset % dev->quantum_size;
        struct quantum_arr *qa = scull_find_node(dev, node_index);
        if (qa == NULL)
                goto out;
        if (qa->data == NULL) {
                qa->data = kmalloc(sizeof(void*) * dev->num_quantum, GFP_KERNEL);
                if (qa->data == NULL)
                        goto out;
                memset(qa->data, 0, sizeof(void*) * dev->num_quantum);
        }
        if (qa->data[quantum_index] == NULL) {
                qa->data[quantum_index] = kmalloc(dev->quantum_size, GFP_KERNEL);
                if (qa->data[quantum_index] == NULL)
                        goto out;
                memset(qa->data[quantum_index], 0, dev->quantum_size);
        }
        if (count + quantum_offset > dev->quantum_size)
                count = dev->quantum_size - quantum_offset;
        if (copy_from_user(qa->data[quantum_index] + quantum_offset, buf, count)) {
                rc = -EFAULT;
                goto out;
        }
        *fpos += count;
        rc = count;

        if (*fpos > dev->size)
                dev->size = *fpos;
out:
        mutex_unlock(&dev->lock);
        return rc;
}

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC)
                return -ENOTTY;
        if (_IOC_NR(cmd) < 0 || _IOC_NR(cmd) > SCULL_IOC_IOC_NR_MAX)
                return -ENOTTY;
        if ((_IOC_DIR(cmd) & _IOC_READ) || (_IOC_DIR(cmd) & _IOC_WRITE)) {
                unsigned long arg_size = _IOC_SIZE(cmd);
                if (arg_size != sizeof(int) || !access_ok((void __user *)arg, arg_size))
                        return -EFAULT;
        }
        int rc = 0;
        size_t tmp;
        switch (cmd) {
        case SCULL_IOC_RESET:
                quantum_size = DEFAULT_QUANTUM_SIZE;
                num_quantum = DEFAULT_NUM_QUANTUM;
                break;
        case SCULL_IOC_SET_NUM_QUANTUM:
                rc = __get_user(num_quantum, (int __user *)arg);
                break;
        case SCULL_IOC_SET_QUANTUM_SIZE:
                rc = __get_user(quantum_size, (int __user *)arg);
                break;
        case SCULL_IOC_TELL_NUM_QUANTUM:
                num_quantum = arg;
                break;
        case SCULL_IOC_TELL_QUANTUM_SIZE:
                quantum_size = arg;
                break;
        case SCULL_IOC_GET_NUM_QUANTUM:
                rc = __put_user(num_quantum, (int __user *)arg);
                break;
        case SCULL_IOC_GET_QUANTUM_SIZE:
                rc = __put_user(quantum_size, (int __user *)arg);
                break;
        case SCULL_IOC_QUERY_NUM_QUANTUM:
                return num_quantum;
        case SCULL_IOC_QUERY_QUANTUM_SIZE:
                return quantum_size;
        case SCULL_IOC_SET_AND_GET_NUM_QUANTUM:
                tmp = num_quantum;
                rc = __get_user(num_quantum, (int __user *)arg);
                if (rc == 0)
                        rc = __put_user(tmp, (int __user *)arg);
                break;
        case SCULL_IOC_SET_AND_GET_QUANTUM_SIZE:
                tmp = quantum_size;
                rc = __get_user(quantum_size, (int __user *)arg);
                if (rc == 0)
                        rc = __put_user(tmp, (int __user *)arg);
                break;
        case SCULL_IOC_TELL_AND_QUERY_NUM_QUANTUM:
                tmp = num_quantum;
                num_quantum = arg;
                return tmp;
        case SCULL_IOC_TELL_AND_QUERY_QUANTUM_SIZE:
                tmp = quantum_size;
                quantum_size = arg;
                return tmp;
        }
        return rc;
}

struct file_operations fops = {
        .owner = THIS_MODULE,
        .read = scull_read,
        .write = scull_write,
        .open = scull_open,
        .release = scull_release,
        .unlocked_ioctl = scull_ioctl,
};

static void scull_init_cdev(struct cdev *cdev, dev_t dev_num)
{
        cdev_init(cdev, &fops);
        cdev->owner = THIS_MODULE;
        int rc = cdev_add(cdev, dev_num, 1);
        if (rc < 0) {
                cdev->owner = NULL; // mark owner to NULL, so we can tell its add fail
                printk(KERN_ERR "[ERROR] cdev_add device(%u, %u) fail\n", MAJOR(dev_num), MINOR(dev_num));
        }
        printk(KERN_INFO "[INFO] cdev_add device(%u, %u) success\n", MAJOR(dev_num), MINOR(dev_num));
}

static void scull_cleanup(void)
{
        if (scull_devices) {
                for (int i = 0; i < num_devices; i++) {
                        if (scull_devices[i].cdev.owner) {
                                printk(KERN_INFO "[INFO] cdev_del device(%u, %u)\n", major, minor + i);
                                scull_trim(&scull_devices[i]);
                                cdev_del(&scull_devices[i].cdev);
                        }
                }
        }
        unregister_chrdev_region(MKDEV(major, minor), num_devices);
}

static void __exit scull_exit(void)
{
        scull_cleanup();
        printk(KERN_INFO "module exit\n");
}

static int __init scull_init(void)
{
        dev_t dev_num;
        int rc = alloc_chrdev_region(&dev_num, minor, num_devices, "scull");
        if (rc < 0) {
                printk(KERN_ERR "[ERROR] alloc_chrdev_region fail\n");
                return rc;
        }
        major = MAJOR(dev_num);
        scull_devices = kmalloc(sizeof(*scull_devices) * num_devices, GFP_KERNEL);
        if (scull_devices == NULL) {
                rc = -ENOMEM;
                goto clean;
        }
        memset(scull_devices, 0, sizeof(*scull_devices) * num_devices);
        for (int i = 0; i < num_devices; i++) {
                scull_devices[i].quantum_size = quantum_size;
                scull_devices[i].num_quantum = num_quantum;
                mutex_init(&scull_devices[i].lock);
                scull_init_cdev(&scull_devices[i].cdev, MKDEV(major, minor+i));
        }
        printk(KERN_INFO "module init\n");
        return 0;
clean:
        scull_cleanup();
        return rc;
}

module_init(scull_init);
module_exit(scull_exit);
