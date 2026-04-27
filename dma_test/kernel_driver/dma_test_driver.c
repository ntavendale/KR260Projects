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
static struct cdev my_device;

#define DRIVER_NAME "dma_fifo_driver"
#define DRIVER_CLASS "dma_fifo_class"

/* Variable for data being written to the driver */
static char buffer[255];
static int buffer_pointer;

/* Forward declaration of function that will initiate the stream */
int play(void);

/* Variable to hold the name of the wav-file to be played */
static char filename [256];

/* Variable that holds a reference to our kthread */
static struct task_struct *reader_thread;

/* Thread function */
static int kthread_reader_fn(void *data) {
 	printk("Thread will play %s",filename);
    play();
    return 0;
}

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
 		printk("I will play %s.\n",filename);
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
 	/* Allocate a device number. Will auto select availble major version.*/
	int allocResult = alloc_chrdev_region(
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
    struct device *my_device = device_create(
		my_class, // Class from abover that device will be registered to
		NULL,  // Parent - doesn't have one
		my_device_nr, // device number
		NULL, 
		DRIVER_NAME);
	if (IS_ERR(my_device)) {
		printk("Can not create device file! Error: %d\n", PTR_ERR(my_device));
		goto FileError;
	}

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Register device to kernel */
	result = cdev_add(&my_device, my_device_nr, 1);
	if (result < 0) {
		printk("Registering of device to kernel failed!. cdev_add returned %d\n", result);
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
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class); 	
	unregister_chrdev(my_device_nr, DRIVER_NAME);
	printk("Removing dma_audio_driver from kernel.\n");
}  

/** 
 **
 ** Functions to Load a wav file
 **
 **/

/* WAV header structure (44 bytes total) */
struct WavHeader {
    char riff[4];               // "RIFF"
    uint32_t chunk_size;        // Overall size of file in bytes minus 8 bytes
    char wave[4];               // "WAVE"
    char fmt[4];                // "fmt " (with a trailing space)
    uint32_t subchunk1_size;    // Size of the fmt chunk (usually 16 for PCM)
    uint16_t audio_format;      // Audio format (1 for PCM)
    uint16_t num_channels;      // Number of channels (1 = mono, 2 = stereo)
    uint32_t sample_rate;       // Sample rate (e.g., 44100 Hz)
    uint32_t byte_rate;         // Byte rate = SampleRate * NumChannels * BitsPerSample/8
    uint16_t block_align;       // Block align = NumChannels * BitsPerSample/8
    uint16_t bits_per_sample;   // Bits per sample (e.g., 16 for 16-bit audio)
    char data[4];               // "data"
    uint32_t data_size;         // Number of bytes in data section
} ;

/* Function to print WAV header information */
void print_wav_header(const struct WavHeader *header) {
    pr_info("WAV File Header:\n");
    pr_info("  Chunk ID: %.4s\n", header->riff);
    pr_info("  Chunk Size: %u\n", header->chunk_size);
    pr_info("  Format: %.4s\n", header->wave);
    pr_info("  Subchunk1 ID: %.4s\n", header->fmt);
    pr_info("  Subchunk1 Size: %u\n", header->subchunk1_size);
    pr_info("  Audio Format: %u\n", header->audio_format);
    pr_info("  Number of Channels: %u\n", header->num_channels);
    pr_info("  Sample Rate: %u\n", header->sample_rate);
    pr_info("  Byte Rate: %u\n", header->byte_rate);
    pr_info("  Block Align: %u\n", header->block_align);
    pr_info("  Bits per Sample: %u\n", header->bits_per_sample);
    pr_info("  Subchunk2 ID: %.4s\n", header->data);
    pr_info("  Data Size: %u\n", header->data_size);
}

/* We need to know the total filesize if we are going to read chunks of the wav file */
static int get_file_size(const char *filename, loff_t *size) {
    struct file *filp;
    struct kstat stat;
    int ret;

    // Open the file
    filp = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        pr_err("dma_audio_driver - Failed to open file %s, error %ld\n", filename, PTR_ERR(filp));
        return PTR_ERR(filp);
    }

    // Get file attributes
    ret = vfs_getattr(&filp->f_path, &stat, STATX_SIZE, AT_STATX_SYNC_AS_STAT);
    if (ret) {
        pr_err("dma_audio_driver - Failed to get attributes for file %s, error %d\n", filename, ret);
        filp_close(filp, NULL);
        return ret;
    }

    *size = stat.size; // Extract the file size
    filp_close(filp, NULL); // Close the file

    return 0;
}

/* Read a chunk of the wav file into memory */
static int get_wav_chunk(size_t start_byte, size_t chunk_size, bool *last, size_t *read_size, u32 *data_buf) {
	struct file *file;
    loff_t offset = 0;
    ssize_t bytes_read;
    loff_t filesize;

    size_t remaining;
    size_t actual_chunk_size;
    
    // Open file
    file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_err("dma_audio_driver - Failed to open WAV file: %s\n", filename);
        return PTR_ERR(file);
    }

    // Get file size
    get_file_size(filename, &filesize);
    if (start_byte >= (filesize - WAV_HEADER_SIZE)) {
        pr_err("dma_audio_driver - Start byte exceeds file size.\n");
        filp_close(file, NULL);
        *last = true;
        *read_size = 0;
        return -EIO;
    }
	if (filesize <= 0){
		pr_err("dma_audio_driver - Filesize of the wav file is %lld\n", filesize);
		return -EIO;
	}
	
    remaining = filesize - WAV_HEADER_SIZE - start_byte;
 	actual_chunk_size = (remaining < chunk_size) ? remaining : chunk_size;

    // Read data chunk
    offset = WAV_HEADER_SIZE + start_byte;
    bytes_read = kernel_read(file, data_buf, actual_chunk_size, &offset);
    if (bytes_read < 0) {
        pr_err("dma_audio_driver - Error reading WAV data chunk.\n");
        filp_close(file, NULL);
        return bytes_read;
    }

    *read_size = bytes_read;
    *last = (start_byte + actual_chunk_size >= (filesize - WAV_HEADER_SIZE));
    filp_close(file, NULL);
    return 0;
}

/* Read the Header information of the wav-file */
static int load_wav_header(size_t *data_size) {
    struct file *file;
    struct WavHeader header;
    loff_t offset = 0;
    ssize_t bytes_read;

    /* Open the file */
    file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_err("dma_audio_driver - Failed to open WAV file: %s\n", filename);
        return PTR_ERR(file);
    }

    /* Read the WAV header */
    bytes_read = kernel_read(file, &header, sizeof(header), &offset);
    if (bytes_read != sizeof(header)) {
        pr_err("dma_audio_driver - Failed to read WAV header\n");
        filp_close(file, NULL);
        return -EIO;
    }

    /* Print header information */
    print_wav_header(&header);

    /* Verify "RIFF" and "WAVE" format */
    if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0) {
        pr_err("dma_audio_driver - Invalid WAV file format\n");
        filp_close(file, NULL);
        return -EINVAL;
    }

    /* Update data size */
    *data_size = header.data_size;

    /* Close the file */
    filp_close(file, NULL);

    pr_info("dma_audio_driver - Loaded WAV file with data size %zu bytes\n", *data_size);
    return 0;
}

/* This function will load the wav-file based on the static variable filename and start dma for each chunk until it is finished */
int play() {
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *chan_desc;
	dma_cookie_t cookie;
	dma_addr_t src_addr, dst_addr;
	u32 *src_buf;
	void *dst_dev;
	struct completion cmp;
	int status;
	int ret;
	
 	size_t start_byte;
	size_t chunk_size;
    size_t read_size;                      // The actual number of bytes read
    size_t data_size;
    bool last;

	struct dma_slave_config slave_config;
		
 	printk("dma_fifo_driver - Init\n");

 	dma_cap_zero(mask);
 	dma_cap_set(DMA_SLAVE|DMA_PRIVATE, mask);
 	// dma17chan0 is the channel used for this dma, check 'ls /sys/class/dma/' to know which channels are available 
 	// dma0 to dma15 are for memory-to-memory dma, dma16 is for memory-to-device)
 	chan = dma_request_channel(mask, NULL, DMA_CHANNEL);
 	if(!chan) {
 		printk("dma_audio_driver - Error requesting dma channel\n");
 		return -ENODEV;
 	}

    /* Attempt to set a 64-bit DMA mask */
    ret = dma_set_mask_and_coherent(chan->device->dev, DMA_BIT_MASK(64));
    if (ret) {
        pr_err("dma_audio_driver - Failed to set 64-bit DMA mask\n");

        /* Fallback to 32-bit DMA mask */
        ret = dma_set_mask_and_coherent(chan->device->dev, DMA_BIT_MASK(32));
        if (ret) {
            pr_err("dma_audio_driver - Failed to set 32-bit DMA mask\n");
            return ret;
        }
    }

    pr_info("dma_audio_driver - DMA mask set successfully\n");
    
    // Allocate coherent memory for source buffer
    src_buf = dma_alloc_coherent(chan->device->dev, BUFFER_SAMPLES, &src_addr, GFP_KERNEL);
    if (!src_buf) {
	    printk("dma_audio_driver - DMA buffer allocation failed\n");
    	status = -ENOMEM;
    	goto free;
    }

    start_byte = 0;           // Starting byte (after the header)
    chunk_size = BUFFER_SAMPLES;       // Size of the chunk to read
    last = false;             // Flag to determine if it's the last chunk
	load_wav_header(&data_size);

	/* Loop over all chunks in the wav-file */
    while (!last) {
        get_wav_chunk(start_byte, chunk_size, &last, &read_size, src_buf);
        if (!src_buf) {
            pr_info("dma_audio_driver - Failed to read chunk.\n");
            break;
        }

		slave_config.direction = DMA_MEM_TO_DEV;
		slave_config.dst_addr = DMA_BASE_ADDRESS; // The address you see in Vivado Address Editor next to DMA AXI Lite
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;  // 32 bits
		
		dmaengine_slave_config(chan, &slave_config);
		/* Request a channel descriptor */
		chan_desc = dmaengine_prep_slave_single(chan, src_addr, BUFFER_SAMPLES, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT);
		if (!chan_desc){
			printk("dma_audio_driver - Error requesting channel descriptor\n");
			status = -1;
			goto free;
		}
		/* Setup wich function needs to be called upon completion */
		init_completion(&cmp);

		chan_desc->callback = my_dma_transfer_completed;
		chan_desc->callback_param = &cmp;

		cookie = dmaengine_submit(chan_desc);

		/* Fire the DMA Transfer */
		dma_async_issue_pending(chan);

		if(wait_for_completion_timeout(&cmp, msecs_to_jiffies(3000)) <= 0) {
			printk("dma_audio_driver - Timeout!\n");
			status = -1;
			goto rel;
		}

		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
		if(status == DMA_COMPLETE) {
			status = 0;	
		} else {
			printk("dma_audio_driver - Error on DMA transfer\n");
		}

        start_byte += read_size;            // Move to the next chunk
	}
rel:
	dmaengine_terminate_all(chan);
free:
	dma_free_coherent(chan->device->dev, BUFFER_SAMPLES, src_buf, src_addr);
	if (dst_dev) {
		iounmap(dst_dev);  // Unmap the device memory
	}
	dma_unmap_single(chan->device->dev, dst_addr, BUFFER_SAMPLES, DMA_TO_DEVICE);

	dma_release_channel(chan);	
	return 0;
}

module_init(dma_fifo_init);
module_exit(dma_fifo_exit);