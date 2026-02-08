#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h> // dev_t
#include <linux/kdev_t.h> // dev_t, MAJOR, MINOR, MKDEV
#include <linux/cdev.h> // struct cdev
#include <linux/string.h> // memset
#include <linux/slab.h> // kmalloc
#include <linux/errno.h> // errcode
#include <linux/fs.h>
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
};

unsigned int major;
unsigned int minor = 0;
unsigned int num_devices = 3; 
size_t quantum_size = 4096; // should pass in module load time later
unsigned int num_quantum = 100;
struct scull_dev *scull_devices;

struct file_operations fops = {
        .owner = THIS_MODULE,
        .read = NULL,
        .write = NULL,
        .open = NULL,
        .release = NULL,
};

static void scull_init_cdev(struct cdev *cdev, dev_t dev_num)
{
        cdev_init(cdev, &fops);
        cdev->owner = THIS_MODULE;
        int rc = cdev_add(cdev, dev_num, 1);
        if (rc < 0) {
                cdev->owner = NULL; // mark owner to NULL, so we can tell its add fail
                printk(KERN_ERR "[ERROR] cdev_add fail, major: %u, minor: %u", MAJOR(dev_num), MINOR(dev_num));
        }
        printk(KERN_INFO "[INFO] cdev_add success, major: %u, minor: %u", MAJOR(dev_num), MINOR(dev_num));
}

static void __exit scull_cleanup(void)
{
        dev_t dev_num = MKDEV(major, minor);
        if (scull_devices) {
                for (int i = 0; i < num_devices; i++) {
                        if (scull_devices[i].cdev.owner) {
                                printk(KERN_INFO "[INFO] cleanup cdev, major: %u, minor: %u", major, minor + i);
                                cdev_del(&scull_devices[i].cdev);
                        }
                }
        }
        unregister_chrdev_region(dev_num, num_devices);
        printk(KERN_INFO "module exit\n");
}

static int __init scull_init(void)
{
        dev_t dev_num;
        int rc = alloc_chrdev_region(&dev_num, minor, num_devices, "scull");
        if (rc < 0) {
                printk(KERN_ERR "[ERROR] alloc_chrdev_region fail");
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
                scull_init_cdev(&scull_devices[i].cdev, MKDEV(major, minor+i));
        }
        printk(KERN_INFO "module init\n");
        return 0;
clean:
        scull_cleanup();
        return rc;
}

module_init(scull_init);
module_exit(scull_cleanup);
