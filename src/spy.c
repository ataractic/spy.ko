#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/input-event-codes.h>

/**
 * Ioctl declarations
*/
#define SPY_TOGGLE_LOGGING _IO(0x44, 0)

/**
 * Global variables
*/
dev_t dev = 0;
static struct class *dev_class;
static struct cdev n1_cdev;
int log_enabled = 0;

/**
 * File operations forward declarations
*/
ssize_t spy_read(struct file *file, char __user *buf, size_t len, loff_t *offp);
ssize_t spy_write(struct file *file, const char __user *buf, size_t len, loff_t *offp);
int spy_open(struct inode *inode, struct file *file);
int spy_release(struct inode *inode, struct file *file);
long int spy_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/**
 * @brief Defines file operations supported by the device
*/
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = spy_read,
    .write = spy_write,
    .open = spy_open,
    .release = spy_release,
    .unlocked_ioctl = spy_ioctl
};

/**
 * Input event handlers forward declarations
*/
void spy_event_callback(struct input_handle *handle, unsigned int type, unsigned int code, int value);
int spy_connect_callback(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id);
void spy_disconnect_callback(struct input_handle *handle);

/**
 * @brief Defines the types of input devices and events that the input handler is interested in handling.
 */
static const struct input_device_id spy_input_id_table[] = {
    { .driver_info = 1 },	/* Matches all devices */
	{ }, /* Terminating null entry */
};

/**
 * @brief Defines event handlers
 */
static struct input_handler spy_handler = {
    .event = spy_event_callback,
    .connect = spy_connect_callback,
    .disconnect = spy_disconnect_callback,
    .name = "spy_input_handler",
    .id_table = spy_input_id_table,
};

/**
 * @brief Gets called whenever an input device event is reported
 * @param handle Handle to the input device
 * @param type Event type (see: <linux/input-event-codes.h>)
 * @param code Event code (see: <linux/input-event-codes.h>)
 * @param value Event value (see: <linux/input-event-codes.h>)
 */
void spy_event_callback(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
    if (log_enabled) {
        pr_info("spy: event: type=%u, code=%u, value=%d, dev=%s, phys=%s\n", type, code, value, handle->dev->name, handle->dev->phys);
    }
}

/**
 * @brief Gets called on input device connection (or detection)
 * @param handler Pointer to managing input_handler struct
 * @param dev Pointer to corresponding input device
 * @param id Pointer to other information concerning the device
 * @return 0 on success
*/
int spy_connect_callback(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id)
{
    struct input_handle *handle;
    int status;

    handle = kmalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle) {
        return -ENOMEM;
    }

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "spy_handle";
    handle->private = NULL;

    status = input_register_handle(handle);
    if (status) {
        goto err_free_handle;
    }

    status = input_open_device(handle);
    if (status) {
        goto err_unregister_handle;
    }

    pr_info("spy: connect_callback: dev=%s, phys=%s\n", dev->name, dev->phys);

    return 0;

err_unregister_handle:
    input_unregister_handle(handle);

err_free_handle:
    kfree(handle);

    pr_err("spy: failed to handle connect callback for device %s\n", dev->name);

    return status;
}

/**
 * @brief Called on input device disconnection
 * @param handle Handle to the input device, created in spy_connect_callback()
 */
void spy_disconnect_callback(struct input_handle *handle)
{
    pr_info("spy: disconnect_callback: dev=%s, phys=%s\n", handle->dev->name, handle->dev->phys);

    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/**
 * @brief Ioctl handler function
 * @param inode File being worked on
 * @param file File pointer
 * @param cmd Ioctl command
 * @param arg Ioctl argument
 * @return 0 on success
*/
long int spy_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case SPY_TOGGLE_LOGGING: {
            log_enabled = !log_enabled;

            pr_info("spy: toggle_logging: enabled=%d\n", log_enabled);

            return 0;
        }
        default: {
            pr_info("spy: ioctl: unknown\n");
            break;
        }
    }

    return 0;
}

/**
 * @brief Gets called when an application read the device file in /dev/<device name>
 * @param file File pointer
 * @param buf User-space data buffer pointer
 * @param len Size of the requested data transfer
 * @param offp Indicates the file position the user is accessing
 * @return Number of bytes read, Negative value on error
*/
ssize_t spy_read(struct file *file, char __user *buf, size_t len, loff_t *offp) {
    pr_info("spy: read\n");

    return 0;
}

/**
 * @brief Gets called when an application writes to the device file in /dev/<device name>
 * @param file File pointer
 * @param buf User-space data buffer pointer
 * @param len Size of the requested data transfer 
 * @param offp Indicates the file position the user is accessing
 * @return Number of bytes written, Negative value on error
*/
ssize_t spy_write(struct file *file, const char __user *buf, size_t len, loff_t *offp) {
    pr_info("spy: write\n");

    return len;
}

/**
 * @brief Gets called when the device file gets opened
 * @param inode File information
 * @param file File pointer
 * @return 0 on success
*/
int spy_open(struct inode *inode, struct file *file) {
    pr_info("spy: open\n");

    return 0;
}

/**
 * @brief Gets called when the device file gets released 
 * @param inode File information
 * @param file File pointer
 * @return 0 on success
*/
int spy_release(struct inode *inode, struct file *file) {
    pr_info("spy: release\n");
    
    return 0;
}

/**
 * @brief Called by the os on module insertion
 * @return 0 on success, -1 otherwise
*/
static int __init spy_init(void) {
    if (alloc_chrdev_region(&dev, 0, 1, "spy") < 0) {
        pr_err("spy: failed to allocate major number\n");
        return -1;
    }

    cdev_init(&n1_cdev, &fops);

    if(cdev_add(&n1_cdev, dev, 1) < 0){
        pr_err("spy: failed to add the device to the system\n");
        goto class_fail;
    }

    dev_class = class_create(THIS_MODULE, "spy");
    if (IS_ERR(dev_class)) {
        pr_err("spy: failed to create struct class for device\n");
        goto class_fail;
    }

    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "spy"))) {
        pr_err("spy: failed to create the device\n");
        goto device_fail;
    }

    if (input_register_handler(&spy_handler)) {
        pr_err("spy: failed to register input handler\n");
        goto device_fail;
    }

    pr_info("spy: loaded: major=%d, minor=%d\n", MAJOR(dev), MINOR(dev));

    return 0;

device_fail:
    device_destroy(dev_class, dev);

class_fail:
    unregister_chrdev_region(dev, 1);
    
    return -1;
}

/**
 * @brief Called by OS on module unload event
*/
static void __exit spy_exit(void) {
    input_unregister_handler(&spy_handler);
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    unregister_chrdev_region(dev, 1);

    pr_info("spy: unloaded\n");
}

module_init(spy_init);
module_exit(spy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mystère <contact@mystere.dev>");
MODULE_DESCRIPTION("spy kernel module");
MODULE_VERSION("0.1");