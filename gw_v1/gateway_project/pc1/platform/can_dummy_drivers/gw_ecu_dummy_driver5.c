#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>

#define MAX_DEV 1
#define CAN_FRAME 8

uint8_t data[8];

typedef struct {
    char *gw2cn_buf;
    char *cn2gw_buf;
    int gw2cn_pkt_arvl;
    int cn2gw_pkt_arvl;
} gw_buff_t;

char buff[8];
char buff1[8];
gw_buff_t sh_buf5;

struct mutex can5_rdlock;
struct mutex can5_wrlock;

EXPORT_SYMBOL(sh_buf5);
EXPORT_SYMBOL(can5_rdlock);
EXPORT_SYMBOL(can5_wrlock);
gw_buff_t *gw_buff = &sh_buf5;

static int gw_ecu_driver_open(struct inode *inode, struct file *file);
static int gw_ecu_driver_release(struct inode *inode, struct file *file);
static long gw_ecu_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t gw_ecu_driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t gw_ecu_driver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations gw_ecu_driver_fops = {
    .owner      = THIS_MODULE,
    .open       = gw_ecu_driver_open,
    .release    = gw_ecu_driver_release,
    .unlocked_ioctl = gw_ecu_driver_ioctl,
    .read       = gw_ecu_driver_read,
    .write       = gw_ecu_driver_write
};

struct mychar_device_data {
    struct cdev cdev;
};

static int dev_major = 0;
static struct class *gw_ecu_driver_class = NULL;
static struct mychar_device_data gw_ecu_driver_data[MAX_DEV];

static int gw_ecu_driver_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init gw_ecu_driver_init(void)
{
    int err, i;
    dev_t dev;

    err = alloc_chrdev_region(&dev, 0, MAX_DEV, "gateway_ecu_dummy_driver5");

    dev_major = MAJOR(dev);

    gw_ecu_driver_class = class_create(THIS_MODULE, "gateway_ecu_dummy_driver5");
    gw_ecu_driver_class->dev_uevent = gw_ecu_driver_uevent;

    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&gw_ecu_driver_data[i].cdev, &gw_ecu_driver_fops);
        gw_ecu_driver_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&gw_ecu_driver_data[i].cdev, MKDEV(dev_major, i), 1);

        device_create(gw_ecu_driver_class, NULL, MKDEV(dev_major, i), NULL, "gateway_ecu_dummy_driver5");
    }
        gw_buff->cn2gw_pkt_arvl = 0;
        gw_buff->gw2cn_pkt_arvl = 0;
    printk("gateway_ecu_dummy_driver5 Initialized\n");

    return 0;
}

static void __exit gw_ecu_driver_exit(void)
{
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(gw_ecu_driver_class, MKDEV(dev_major, i));
    }

    class_unregister(gw_ecu_driver_class);
    class_destroy(gw_ecu_driver_class);

    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);
}

static int gw_ecu_driver_open(struct inode *inode, struct file *file)
{
    gw_buff->gw2cn_buf = buff;
    gw_buff->cn2gw_buf = buff1;
 mutex_init(&can5_rdlock);
    mutex_init(&can5_wrlock);
    printk("gateway_ecu_dummy_driver5 : open\n");
    return 0;
}

static int gw_ecu_driver_release(struct inode *inode, struct file *file)
{
    printk("gateway_ecu_dummy_driver5 : close\n");
    return 0;
}

static long gw_ecu_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    printk("gateway_ecu_dummy_driver5 : ioctl\n");
    return (long) (gw_buff);
}

static ssize_t gw_ecu_driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{

    mutex_lock(&can5_wrlock);

    if (gw_buff->cn2gw_pkt_arvl == 1)
    {
        if (copy_to_user(buf, gw_buff->cn2gw_buf, count)) {
            count = 0;
            return count;
        }
        //printk("gw_ecu_driver read : CAN-DATA : (%s)\n", gw_buff->cn2gw_buf);
        memset(gw_buff->cn2gw_buf, 0, 8);
        gw_buff->cn2gw_pkt_arvl = 0;
    }
    else
    {
        count = 0;
    }
    mutex_unlock(&can5_wrlock);

    return count; 
}

static ssize_t gw_ecu_driver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    size_t ncopied;
    mutex_lock(&can5_rdlock);

    if( 0 == gw_buff->gw2cn_pkt_arvl)
    {
        memset(gw_buff->gw2cn_buf, '\0', 8);
        ncopied = copy_from_user(gw_buff->gw2cn_buf, buf, count);

        if (ncopied == 0) {
           // printk("gw_ecu_driver write : CAN-DATA : (%s)\n", gw_buff->gw2cn_buf);
            gw_buff->gw2cn_pkt_arvl = 1;
        } else {
           // printk("Could't copy %zd bytes from the user\n", ncopied);
            count = 0;
        }
    }
    mutex_unlock(&can5_rdlock);

    return count;
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("PAVAN");

module_init(gw_ecu_driver_init);
module_exit(gw_ecu_driver_exit);


