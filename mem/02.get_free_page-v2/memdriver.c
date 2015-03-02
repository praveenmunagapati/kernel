#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gfp.h>

/* module information */
MODULE_AUTHOR("Troy D. Hanson");
MODULE_DESCRIPTION("Example of character device");
MODULE_LICENSE("Dual BSD/GPL");

#define NUM_MINORS 1

/* a global to keep state. must be thread safe. */
struct chardev_t {
  dev_t dev;        /* has major and minor bits */
  struct cdev cdev; /* has our ops, owner, etc */

  /* we have a single buffer shared by all users */
  spinlock_t lock;     // TODO declare this right
  unsigned long page;  // virtual address of page
  size_t bytes_available;  // bytes available to read 
} c;

int _open(struct inode *inode, struct file *f) {
  return 0;
}

int _release(struct inode *inode, struct file *f) {
  return 0;
}

ssize_t _read(struct file *f, char __user *buf, size_t count, loff_t *offp) {
  // XXX spinlock
  size_t sz = (count > c.bytes_available) ? c.bytes_available : count;
  if (copy_to_user(buf, (void*)c.page, sz)) return -EFAULT;
  *offp += sz;
  c.bytes_available -= sz;
  return sz;
}

ssize_t _write(struct file *f, const char __user *buf, size_t count, 
               loff_t *offp) {
  // XXX spinlock
  size_t sz = (count > PAGE_SIZE) ? PAGE_SIZE : count;
  if (copy_from_user((void*)c.page, buf, sz)) return -EFAULT;
  *offp += sz;
  c.bytes_available = sz;
  return sz;
}

struct file_operations ops = {
  .read = _read,
  .write = _write,
  .unlocked_ioctl = NULL,
  .open = _open,
  .release = _release
};

int __init chardev_init(void) {
  int rc;

  /* get the page that is our shared buffer */
  c.page = __get_free_page(GFP_KERNEL);
  if (!c.page) {rc = -ENOMEM; goto done; }
  printk(KERN_DEBUG " page virt addr %p\n", (void*)c.page);

  /* ask for a dynamic major */
  rc = alloc_chrdev_region(&c.dev, 0, NUM_MINORS, "memdriver");
  if (rc) { rc = -ENODEV; free_page(c.page); goto done; }

  /* init the struct cdev */
  cdev_init(&c.cdev, &ops);
  c.cdev.owner = THIS_MODULE;

  /* make device live */
  rc = cdev_add(&c.cdev, c.dev, NUM_MINORS);
  if (rc) {
    printk(KERN_WARNING "cdev_add: can't add device\n");
    unregister_chrdev_region(c.dev, NUM_MINORS);
    cdev_del(&c.cdev);
    free_page(c.page);
    rc = -ENODEV;
    goto done;
  }

  rc = 0;

 done:
  return rc;
}

void __exit chardev_cleanup(void) {
  free_page(c.page);
  cdev_del(&c.cdev);
  unregister_chrdev_region(c.dev, NUM_MINORS);
}

module_init(chardev_init);
module_exit(chardev_cleanup);
