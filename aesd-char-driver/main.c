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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Sharath Jonnala"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{

	struct aesd_dev *aesd_dev_ptr = NULL;

	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	aesd_dev_ptr = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = aesd_dev_ptr;
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_dev *aesd_dev_ptr = filp->private_data;
	size_t buf_element_offset = 0;
	struct aesd_buffer_entry * buff_element;
	int missed_bytes = 0;

	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle read
	 */


	if (mutex_lock_interruptible(&aesd_dev_ptr->lock))
	{
		mutex_unlock(&aesd_dev_ptr->lock);
		return -ERESTARTSYS;
	}

	if(count <= 0)
	{
	    mutex_unlock(&aesd_dev_ptr->lock);
	    return 0;
	}


	buff_element =  aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_dev_ptr->cb, *f_pos, &buf_element_offset);

	if(buff_element == NULL)
	{
	    mutex_unlock(&aesd_dev_ptr->lock);
	    return 0;
	}

	if(buff_element->size - buf_element_offset >= count)
	{
	    missed_bytes = copy_to_user(buf, (buff_element->buffptr + buf_element_offset), count);
            *f_pos += count - missed_bytes;
            mutex_unlock(&aesd_dev_ptr->lock);
	    if(missed_bytes > 0)
            {
                return -EFAULT;
            }
	} else {
		missed_bytes = copy_to_user(buf, (buff_element->buffptr + buf_element_offset), buff_element-> size - buf_element_offset);
		*f_pos += buff_element-> size - buf_element_offset - missed_bytes;
        	mutex_unlock(&aesd_dev_ptr->lock);
		if(missed_bytes > 0)
        	{
        	    return -EFAULT;
	        }

		return (buff_element-> size - buf_element_offset);

	}
	return 0;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	size_t missed_bytes = 0;
	char* linebreak_ptr = NULL;
	const char* overwritten_byte = NULL;
	struct aesd_dev* aesd_dev_ptr = (filp->private_data);

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */



	if (mutex_lock_interruptible(&aesd_dev_ptr->lock)) {
		return -ERESTARTSYS;
	}

	// If no data saved, malloc new buffer, else realloc
	if (aesd_dev_ptr->buffer_element.size == 0) {
		aesd_dev_ptr->buffer_element.buffptr = kmalloc(count, GFP_KERNEL);
		if (aesd_dev_ptr->buffer_element.buffptr == 0) {
			mutex_unlock(&aesd_dev_ptr->lock);
			return -ENOMEM;
		}
	}
	
	// Save buffer into temp_entry
	missed_bytes = copy_from_user((void *)(&aesd_dev_ptr->buffer_element.buffptr[aesd_dev_ptr->buffer_element.size]), buf, count);
	aesd_dev_ptr->buffer_element.size += count - missed_bytes;

	linebreak_ptr = (char *)memchr(aesd_dev_ptr->buffer_element.buffptr, '\n', aesd_dev_ptr->buffer_element.size);
	// Write to queue if newline is found
	if (linebreak_ptr) {
		// Add entry to queue, free oldest entry if full
		overwritten_byte = aesd_circular_buffer_add_entry(&aesd_dev_ptr->cb, &aesd_dev_ptr->buffer_element);
		if (overwritten_byte) {
			kfree(overwritten_byte);
		}
        	aesd_dev_ptr->buffer_element.buffptr = 0;
		aesd_dev_ptr->buffer_element.size = 0;
	}

    //write from beginning
    *f_pos = 0;
    mutex_unlock(&aesd_dev_ptr->lock);
    return (count - missed_bytes);
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */

	mutex_init(&aesd_device.lock);
	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */

	aesd_circular_buffer_free(&aesd_device.cb);

	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
