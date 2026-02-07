#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("scull impl");

unsigned int major;
unsigned int minor = 0;
unsigned int num_devices = 3; 

static int __init scull_init(void)
{
        dev_t dev_num;
        int rc = alloc_chrdev_region(&dev_num, minor, num_devices, "scull");
        if (rc < 0) {
                printk(KERN_WARNING);
                return rc;
        }
        major = MAJOR(dev_num);
        printk(KERN_INFO "get major number: %d", major);
        printk(KERN_WARNING "module init\n");
        return 0;
}

static void __exit scull_exit(void)
{
        dev_t dev_num = MKDEV(major, minor);
        unregister_chrdev_region(dev_num, num_devices);
        printk(KERN_INFO "free character device region\n");
        printk(KERN_WARNING "module exit\n");
}

module_init(scull_init);
module_exit(scull_exit);
