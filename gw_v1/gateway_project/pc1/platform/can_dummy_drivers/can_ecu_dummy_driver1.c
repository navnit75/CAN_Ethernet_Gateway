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

typedef struct {
        char *gw2cn_buf;
        char *cn2gw_buf;
        int gw2cn_pkt_arvl;
        int cn2gw_pkt_arvl;
} gw_buff_t;

extern gw_buff_t sh_buf1;

extern struct mutex can1_rdlock;
extern struct mutex can1_wrlock;

gw_buff_t *gw_buff = &sh_buf1;

static int can_ecu_driver_open(struct inode *inode, struct file *file);
static int can_ecu_driver_release(struct inode *inode, struct file *file);
static long can_ecu_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t can_ecu_driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t can_ecu_driver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static const struct file_operations can_ecu_driver_fops = {
        .owner      = THIS_MODULE,
        .open       = can_ecu_driver_open,
        .release    = can_ecu_driver_release,
        .unlocked_ioctl = can_ecu_driver_ioctl,
        .read       = can_ecu_driver_read,
        .write       = can_ecu_driver_write
};

struct mychar_device_data {
        struct cdev cdev;
};

static int dev_major = 0;
static struct class *can_ecu_driver_class = NULL;
static struct mychar_device_data can_ecu_driver_data[MAX_DEV];

static int can_ecu_driver_uevent(struct device *dev, struct kobj_uevent_env *env)
{
        add_uevent_var(env, "DEVMODE=%#o", 0666);
        return 0;
}

static int __init can_ecu_driver_init(void)
{
        int err, i;
        dev_t dev;

        err = alloc_chrdev_region(&dev, 0, MAX_DEV, "can_ecu_dummy_driver1");

        dev_major = MAJOR(dev);

        can_ecu_driver_class = class_create(THIS_MODULE, "can_ecu_dummy_driver1");
        can_ecu_driver_class->dev_uevent = can_ecu_driver_uevent;

        for (i = 0; i < MAX_DEV; i++) {
                cdev_init(&can_ecu_driver_data[i].cdev, &can_ecu_driver_fops);
                can_ecu_driver_data[i].cdev.owner = THIS_MODULE;

                cdev_add(&can_ecu_driver_data[i].cdev, MKDEV(dev_major, i), 1);

                device_create(can_ecu_driver_class, NULL, MKDEV(dev_major, i), NULL, "can_ecu_dummy_driver1");
        }
        printk("can_ecu_dummy_driver1 Initialized \n");

        return 0;
}

static void __exit can_ecu_driver_exit(void)
{
        int i;

        for (i = 0; i < MAX_DEV; i++) {
                device_destroy(can_ecu_driver_class, MKDEV(dev_major, i));
        }

        class_unregister(can_ecu_driver_class);
        class_destroy(can_ecu_driver_class);

        unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);
}

static int can_ecu_driver_open(struct inode *inode, struct file *file)
{
        printk("can_ecu_dummy_driver1: open\n");
        return 0;
}

static int can_ecu_driver_release(struct inode *inode, struct file *file)
{
        printk("can_ecu_dummy_driver1: close\n");
        return 0;
}

static long can_ecu_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        //gw_buff = (gw_buff_t *)arg;
        printk("can_ecu_dummy_driver1: ioctl \n");
        return 0;
}

static ssize_t can_ecu_driver_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{

        // printk("Reading from can_ecu_dummy_driver1\n"); //: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));
        mutex_lock(&can1_rdlock);
        if(gw_buff->gw2cn_pkt_arvl == 1)
        {
                if (copy_to_user(buf, gw_buff->gw2cn_buf, count)) {
                        count = 0;
                        return count;
                }
                //printk("can_ecu_driver read : CAN-DATA (%s)\n", gw_buff->gw2cn_buf);
                memset(gw_buff->gw2cn_buf, 0, 8);
                gw_buff->gw2cn_pkt_arvl = 0;
        }
        else
        {
                count = 0;
        }
        mutex_unlock(&can1_rdlock);
        return count;
}

static ssize_t can_ecu_driver_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
        size_t ncopied;

        // printk("Writing to can_ecu_dummy_driver1\n"); // %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

        mutex_lock(&can1_wrlock);
        if ( 0 == gw_buff->cn2gw_pkt_arvl)
        {
                memset(gw_buff->cn2gw_buf, 0, 8);
                ncopied = copy_from_user(gw_buff->cn2gw_buf, buf, count);
                if (ncopied == 0) {
                        // printk("can_ecu_driver write : CAN-DATA (%s)\n", gw_buff->cn2gw_buf);
                        gw_buff->cn2gw_pkt_arvl = 1;
                } else {
                        //printk("Could't copy %zd bytes from the user\n", ncopied);
                        count = 0;
                }
                //printk("can_ecu_driver Write : CAN-DATA (%s)\n", gw_buff->cn2gw_buf);
        }

        mutex_unlock(&can1_wrlock);
        return count;
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("PAVAN");

module_init(can_ecu_driver_init);
module_exit(can_ecu_driver_exit);

