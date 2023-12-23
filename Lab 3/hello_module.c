#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

#define PROC_FILENAME "tsu"
static struct proc_dir_entry *proc_entry;

static ssize_t procfile_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    ssize_t ret;
    const char data[] = "Tomsk\n";
    size_t data_size = strlen(data);

    if (*offset >= data_size)
        return 0;  // EOF

    ret = copy_to_user(buffer, data + *offset, data_size - *offset);
    if (ret != 0) {
        pr_err("Failed to copy data to user space\n");
        return -EFAULT;
    }

    *offset += data_size;
    return data_size;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops procfile_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations procfile_fops = {
    .read = procfile_read,
};
#endif

static int __init hello_init(void) {
    pr_info("Welcome to the Tomsk State University\n");

    // Create the /proc file
    proc_entry = proc_create(PROC_FILENAME, 0444, NULL, &procfile_fops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }

    return 0;
}

static void __exit hello_exit(void) {
    pr_info("Unloading the TSU Linux Module\n");

    // Remove the /proc file
    if (proc_entry)
        remove_proc_entry(PROC_FILENAME, NULL);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
