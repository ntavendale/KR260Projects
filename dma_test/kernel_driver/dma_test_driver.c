#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/path.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/string.h>

/* Meta Information for the Linux Kernel Module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nigel Tavendale");
MODULE_DESCRIPTION("A DMA memory copy example to move data to an AXI Stream on the FPGA.");

#define DMA_BASE_ADDRESS    0xA0000000
#define DMA_CHANNEL_WR      "dma17chan0"
#define DMA_CHANNEL_RD      "dma17chan1"

#define WAV_HEADER_SIZE 	44 		// WAV header size (standard 44 bytes)
#define AUDIO_SAMPLE_RATE 	48000	// Play at 48KHz
#define AUDIO_BIT_DEPTH		16 		// 16 bit audio
#define AUDIO_CHANNELS		2 		// Stereo audio
#define BUFFER_SAMPLES		48000 	// Buffer and transfer 1 second of audio each time
#define SINE_FREQUENCY		440 	// frequency to play an A note

/* Variables for device and device class */
static dev_t my_device_nr; // Holds information about the device when registration succsessful
static struct class *my_class;
static struct cdev my_cdev;

#define DRIVER_NAME "dma_test_driver"
#define DRIVER_CLASS "dma_test_class"

/* Variable for data being written to the driver */
static char buffer[255];
static int buffer_pointer;
static char filename [256];

/* Variable that holds a reference to our kthread */
static struct task_struct *reader_thread;
static struct device *my_device;

/* Thread function */
static int kthread_reader_fn(void *data) {
 	printk("kthread_reader_fn");
    return 0;
}

unsigned int umin(unsigned int a, unsigned int b);

/**
 * @brief Read data out of the buffer
 */
static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs){
 	int to_copy, not_copied, delta;
 	/* Get amount of data to copy */
 	to_copy = umin(count, buffer_pointer);
 	/* Copy data to user */
 	not_copied = copy_to_user(user_buffer, buffer, to_copy);
 	/* Calculate data */
 	delta = to_copy - not_copied;
	return delta;
 }

/**
 * @brief Write data out of the buffer
 */
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) 
{
 	int to_copy, not_copied, delta;
 	/* Get amount of data to copy */
 	to_copy = umin(count, sizeof(buffer));
 	/* Copy data to user */
 	not_copied = copy_from_user(buffer, user_buffer, to_copy);
 	buffer_pointer = to_copy;
 	/* Calculate data */
 	delta = to_copy - not_copied;
 	if (not_copied==0){
 		buffer[to_copy]='\0';
	 	snprintf(filename, to_copy, "%s", buffer); 
	    /* Create and start the kernel thread */
	    reader_thread = kthread_run(kthread_reader_fn, NULL, "dma_fifo_thread");
	    if (IS_ERR(reader_thread)) {
	        pr_err("Failed to create kernel thread\n");
	        return PTR_ERR(reader_thread);
	    }
 	}
	return delta;
 }
 
void my_dma_transfer_completed(void *param){
	struct completion *cmp = (struct completion *) param;
	complete(cmp);
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance){
	printk("dev_nr - open was called!\n");
	return 0;
}

/**
 * @brief This function is called, when the device file is closed
 */
static int driver_close(struct inode *device_file, struct file *instance){
	printk("dev_nr - close was called!\n");
	return 0;
}

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
	.write = driver_write
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init dma_fifo_init(void) {
	int allocResult, addResult;
 	/* Allocate a device number. Will auto select availble major version.*/
	allocResult = alloc_chrdev_region(
		&my_device_nr, // Struct to hold device registration
		0,             // Tell it to start with minor version 0
		1,             // Only need 1 minor version number
		DRIVER_NAME);
	if (allocResult < 0) {
		printk("Device nr could not be allocated! Error Code %d\n", allocResult);
		return -1;
	}
	printk("Device nr Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

	/*
	  class_create is a macro, or helper function, which allocates the class struct.
	  This structure acts as a high-level container in sysfs (usually under /sys/class/)
	*/
	my_class = class_create(THIS_MODULE, DRIVER_CLASS);
	if (NULL == my_class) {
		printk("Device class cannot be created!\n");
		goto ClassError;
	}

	/* 
	  Now we create the actual device and register it with sysfs. 
	*/
    my_device = device_create(
		my_class, // Class from abover that device will be registered to
		NULL,  // Parent - doesn't have one
		my_device_nr, // device number
		NULL, 
		DRIVER_NAME);
	if (IS_ERR(my_device)) {
		printk("Can not create device file! Error: %ld\n", PTR_ERR(my_device));
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_cdev, &fops);

	/* Register device to kernel */
	addResult = cdev_add(&my_cdev, my_device_nr, 1);
	if (addResult < 0) {
		printk("Registering of device to kernel failed!. cdev_add returned %d\n", addResult);
		goto Add_Error;
	}
	 	
	printk("Inserted dma_fifo_driver into kernel.\n");
 	return 0;
Add_Error:
	device_destroy(my_class, my_device_nr);
FileError:
	class_destroy(my_class); 	
ClassError:
	unregister_chrdev(my_device_nr, DRIVER_NAME);
	return -1;
 }

 /**
  * @brief This function is called, when the module is removed from the kernel
  */
static void __exit dma_fifo_exit(void) {
	cdev_del(&my_cdev);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class); 	
	unregister_chrdev(my_device_nr, DRIVER_NAME);
	printk("Removing dma_test_driver from kernel.\n");
}  

unsigned int umin(unsigned int a, unsigned int b) {
    return (a < b) ? a : b;
}

module_init(dma_fifo_init);
module_exit(dma_fifo_exit);