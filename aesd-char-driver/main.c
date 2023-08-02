/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"
#include "aesdchar.h"
#include <asm/uaccess.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("erammos"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
int aesd_open(struct inode *inode, struct file *filp) {
  struct aesd_dev *dev;
  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = dev;
  return 0;
}

int aesd_release(struct inode *inode, struct file *filp) { return 0; }

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos) {
  ssize_t retval = 0;
  struct aesd_dev *dev;
  struct aesd_buffer_entry *entry;
  size_t entry_offset_byte_rtn = 0;

  dev = filp->private_data;
  if (mutex_lock_interruptible(&dev->mut)) {
    return -ERESTARTSYS;
  }
  entry = aesd_circular_buffer_find_entry_offset_for_fpos(
      &dev->buffer, *f_pos, &entry_offset_byte_rtn);
  if (entry == NULL) {
    retval = 0;
    goto out;
  } else {
    retval = entry->size - entry_offset_byte_rtn;
    if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, retval) ==
        0) {
        PDEBUG("read f_pos: %lld",*f_pos);
      *f_pos += retval;
    } else {
      PDEBUG("failed copy %lld", *f_pos);
      retval = -EFAULT;
      goto out;
    }
  }
out:
  mutex_unlock(&dev->mut);
  return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos) {
  ssize_t retval = -ENOMEM;
  struct aesd_dev *dev;

  dev = filp->private_data;

  if (mutex_lock_interruptible(&dev->mut)) {
    return -ERESTARTSYS;
  }
  dev->working_entry.buffptr = krealloc(
      dev->working_entry.buffptr, dev->working_entry.size + count, GFP_KERNEL);

  if (dev->working_entry.buffptr) {
    if (copy_from_user(
            (void *)(dev->working_entry.buffptr + dev->working_entry.size), buf,
            count) == 0) {
      dev->working_entry.size += count;
      retval = count;
      *f_pos += retval;
      int found_ret = 0;
      int i = 0;
      for (; i < dev->working_entry.size; i++) {
        if (dev->working_entry.buffptr[i] == '\n') {
          found_ret = 1;
          break;
        }
      }
      if (found_ret) {
        const char *buf =
            aesd_circular_buffer_add_entry(&dev->buffer, &dev->working_entry);
        dev->working_entry.size = 0;
        dev->working_entry.buffptr = 0;

        if (buf != NULL) {
          kfree(buf);
        }
      }
    } else {

      retval = -EFAULT;
      PDEBUG("Cannot copy from user");
      goto out;
    }
  } else {
    retval = -EFAULT;
    PDEBUG("Cannot allocate memory");
    goto out;
  }

out:
  mutex_unlock(&dev->mut);
  /**
   * TODO: handle write
   */
  return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence) {
  struct aesd_dev *dev = filp->private_data;
  return fixed_size_llseek(filp, off, whence, dev->buffer.totalSize);
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  struct aesd_dev *dev = filp->private_data;
  int retval = 0;
  size_t offset = -1;
  uint8_t out = dev->buffer.out_offs;
  size_t totalSize = 0;
  struct aesd_seekto seekto;

  if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
    return -ENOTTY;
  if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
    return -ENOTTY;

  if (cmd == AESDCHAR_IOCSEEKTO) {

  if (mutex_lock_interruptible(&dev->mut)) {
    return -ERESTARTSYS;
  }
    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) !=
        0) {
       PDEBUG("Cannot copy to user");
      retval = -EFAULT;
      goto out;
    }

    if (!dev->buffer.full && out == dev->buffer.in_offs) {
      PDEBUG("Buffer is empty");
      retval = -EINVAL;
      goto out;
    }
    while (out != dev->buffer.in_offs || !totalSize) {

      if (out == (dev->buffer.out_offs + seekto.write_cmd) % 10) {

        if (dev->buffer.entry[out].size <= seekto.write_cmd_offset) {
          PDEBUG("cmd offset out of bound: %u size: %zu",
                 seekto.write_cmd_offset,dev->buffer.entry[out].size);
          retval = -EINVAL;
          goto out;
        }
        offset = totalSize + seekto.write_cmd_offset;
        break;
      }
      totalSize += dev->buffer.entry[out].size;
      out = (out + 1) % 10;
    }
    if (offset < 0) {
      PDEBUG("Offset is negative %zu",offset);
      retval = -EINVAL;
      goto out;
    }
    PDEBUG("ioctl offset is set to %zu",offset);
    filp->f_pos = offset;
    retval = 0;
  }
out:
  mutex_unlock(&dev->mut);
  return retval;
}
struct file_operations aesd_fops = {.owner = THIS_MODULE,
                                    .llseek = aesd_llseek,
                                    .read = aesd_read,
                                    .write = aesd_write,
                                    .open = aesd_open,
                                    .release = aesd_release,
                                    .unlocked_ioctl = aesd_ioctl};

static int aesd_setup_cdev(struct aesd_dev *dev) {
  int err, devno = MKDEV(aesd_major, aesd_minor);

  cdev_init(&dev->cdev, &aesd_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &aesd_fops;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding aesd cdev", err);
  }
  return err;
}

int aesd_init_module(void) {
  dev_t dev = 0;
  int result;
  result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
  aesd_major = MAJOR(dev);
  if (result < 0) {
    printk(KERN_WARNING "Can't get major %d\n", aesd_major);
    return result;
  }
  memset(&aesd_device, 0, sizeof(struct aesd_dev));

  aesd_circular_buffer_init(&aesd_device.buffer);

  aesd_device.working_entry.size = 0;
  aesd_device.working_entry.buffptr = NULL;
  mutex_init(&aesd_device.mut);

  result = aesd_setup_cdev(&aesd_device);

  if (result) {
    unregister_chrdev_region(dev, 1);
  }
  return result;
}

void aesd_cleanup_module(void) {
  dev_t devno = MKDEV(aesd_major, aesd_minor);
  int index = 0;
  struct aesd_buffer_entry *entry;
  cdev_del(&aesd_device.cdev);

  AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
    kfree(entry->buffptr);
  }
  kfree(aesd_device.working_entry.buffptr);
  mutex_destroy(&aesd_device.mut);
  aesd_device.working_entry.buffptr = NULL;
  aesd_device.working_entry.size = 0;
  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
