#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/pci.h>
#include <linux/param.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>

#include "message.h"

#define DEVICE_NAME   "Squeue"  // device name to be created and registered
#define ARRAYP_SIZE   11        //default queue length is 10 and one empty cell is added for circular buffer
#define NUM_DEVICE    4         // four device drivers


/* per device structure */
struct my_dev
{
  struct cdev cdev;                        /* The cdev structure */
  char name[20];                          /* Name of device*/
  struct message *arrayp[ARRAYP_SIZE];   /* Message queue*/
  int start;                             /* Queue start pointer index*/
  int end;                               /* Queue end pointer index*/
  struct semaphore semaph;               /*For queue locking*/
}*my_devp[NUM_DEVICE];

static dev_t my_dev_number; /* Allotted device number */
struct class *my_dev_class; /* Tie with the device model */

typedef unsigned long long ticks;

uint64_t tsc(void)                     // to convert 32 bit timestamp output into 64 bits
{
     uint32_t a, d;
     //asm("cpuid");
     asm volatile("rdtsc" : "=a" (a), "=d" (d));

	return (( (uint64_t)a)|( (uint64_t)d)<<32 );
}


/* ************ Open squeue driver**************** */


int squeue_driver_open(struct inode *inode, struct file *file)
{
  struct my_dev* ptr1;
  printk(KERN_INFO "Driver: open()\n");
  /* Get the per-device structure that contains this cdev */
  ptr1 = container_of(inode->i_cdev, struct my_dev, cdev);

  /* Easy access to cmos_devp from rest of the entry points */
  file->private_data = ptr1;
  printk("%s: Device opening\n", ptr1->name);
  return 0;
}


/* ********* Release squeue driver**************** */


int squeue_driver_release(struct inode *inode, struct file *file)
{
  struct my_dev* ptr2;
  printk(KERN_INFO "Driver: close()\n");
  ptr2 = file->private_data;
  printk("%s: Device closing\n", ptr2->name);

  return 0;
}

/* ************ Read to squeue driver*************** */

ssize_t squeue_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
  int res = 0;
  struct message* temp;
  ssize_t size;
  struct my_dev* ptr3;
	uint64_t tempt;
  // errno = 0;
  size = (sizeof(struct message) + 80);
  ptr3 = file->private_data;
  printk(KERN_INFO "Driver: read()\n");
  temp = (struct message*) kmalloc(sizeof(struct message), GFP_KERNEL);
  
  if (ptr3->start == ptr3->end)
  {
    printk("Queue is empty");
    // errno = EINVAL;
    return (-1);
  }

  down(&ptr3->semaph);                                    // aquire the lock
  temp = (struct message*) kmalloc(sizeof(struct message), GFP_KERNEL);
  temp = ptr3->arrayp[ptr3->start];
  tempt=tsc();
  temp->queueing_time=(temp->queueing_time)+(tempt-temp->current_time);
  temp->current_time=tempt;
  res = copy_to_user(buf, temp, sizeof(struct message)); // Copying to user space
  ptr3->start = (ptr3->start + 1) % ARRAYP_SIZE;          // Circular buffer
  printk(KERN_INFO "Pointer start: %d\n", ptr3->start);
  printk(
      "\n\n\n DEQUED Message ID is \t%d Source ID is \t%d Destination ID is \t%d string element is \t%s\n\n\n",
      temp->message_id, temp->source_id, temp->destination_id, temp->c_string);

  kfree(temp);
  up(&ptr3->semaph);

  return res;

}

/* ************* Write to squeue driver**************** */

ssize_t squeue_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  uint64_t ct;
	int res =0;
  struct message* msg;
  struct my_dev *ptr4;
  ptr4 = file->private_data;
  printk(KERN_INFO "Driver: write()\n");
  if ((ptr4->end + 1) % ARRAYP_SIZE == ptr4->start)
  {
    printk(KERN_INFO "Queue is Full");
    return -1;
  }
  msg = (struct message*) kmalloc(sizeof(struct message), GFP_KERNEL);

  down(&ptr4->semaph);                             // aquire the lock                                       
  ct=tsc();					// to record current time
  res = copy_from_user(msg, buf, sizeof(struct message)); 
	msg->current_time = ct;        // copy from user
  ptr4->arrayp[ptr4->end] = msg;
  printk(KERN_INFO "Pointer end: %d\n", ptr4->end);
  printk("Enqueued Message IS:: MID:\t%d\tSID:%d\tRID:%d\t %s\n",
      ptr4->arrayp[ptr4->end]->message_id, ptr4->arrayp[ptr4->end]->source_id,
      ptr4->arrayp[ptr4->end]->destination_id,
      ptr4->arrayp[ptr4->end]->c_string);

  ptr4->end = (ptr4->end + 1) % ARRAYP_SIZE;                       
  up(&ptr4->semaph);                                       // release the lock   
  return res;

}	

/* File operations structure. Defined in linux/fs.h */
static struct file_operations squeue_fops = {
  .owner = THIS_MODULE, /* Owner */
  .open = squeue_driver_open, /* Open method */
  .release = squeue_driver_release, /* Release method */
  .write = squeue_driver_write, /* Write method */
  .read = squeue_driver_read, /* Read method */
};

/* **************** Driver Initialization****************** */

int __init squeue_driver_init(void)
{
  int ret;
  int i;
  int j;

  /* Request dynamic allocation of a device major number */
  if (alloc_chrdev_region(&my_dev_number, 0, 4, DEVICE_NAME) < 0)
  {
    printk(KERN_DEBUG "Can't register device\n"); return -1;
  }

  /* Populate sysfs entries */
  my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

  for (i = 0; i < NUM_DEVICE; i++)
  {
    /* Allocate memory for the per-device structure */
    my_devp[i] = kmalloc(sizeof(struct my_dev), GFP_KERNEL);
    if (!my_devp[i])
    {
      printk("Bad Kmalloc\n"); return -ENOMEM;
    }

    if ( i==0)
    {
    strcpy( my_devp[i]->name, "bus_in_q");
    //sprintf(my_devp[i]->name, "%s%d",DEVICE_NAME,i);
    }
    else if (i==1)
    strcpy( my_devp[i]->name, "bus_in_q1");
    else if (i==2)
    strcpy( my_devp[i]->name, "bus_in_q2");
    else if (i==3)
    strcpy( my_devp[i]->name, "bus_in_q3");

    /* Connect the file operations with the cdev */
    cdev_init(&my_devp[i]->cdev, &squeue_fops);
    my_devp[i]->cdev.owner = THIS_MODULE;

    /* Connect the major/minor number to the cdev */
    ret = cdev_add(&my_devp[i]->cdev, (MKDEV(MAJOR(my_dev_number), MINOR(my_dev_number)+i)), NUM_DEVICE);

    if (ret)
    {
      printk("Bad cdev\n");
      return ret;
    }

    /* Send uevents to udev, so it'll create /dev nodes */
    if ( i==0)
    device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), MINOR(my_dev_number)+i), NULL, "%s","bus_in_q");
    else if (i==1)
    device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), MINOR(my_dev_number)+i), NULL, "%s","bus_in_q1");
    else if (i==2)
    device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), MINOR(my_dev_number)+i), NULL, "%s","bus_in_q2");
    else if (i==3)
    device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), MINOR(my_dev_number)+i), NULL, "%s","bus_in_q3");

    /* initializing the queue start and end with zero*/
    for(j = 0; j < ARRAYP_SIZE; j++)
      my_devp[i]->arrayp[j] = NULL;
    my_devp[i]->start = 0;
    my_devp[i]->end = 0;
    printk("\n\n\n\n\nREAR:%d\tFRONT::%d\n\n\n",my_devp[i]->end,my_devp[i]->start);
    //my_devp[i]->arrayp[0]= kcalloc(ARRAYP_SIZE, sizeof(struct message), GFP_KERNEL);

    /* initializing the semaphores */
    sema_init(&my_devp[i]->semaph,1);

  }

  printk("Driver initialized.\n");

  return 0;
}

/* ******************Driver Exit******************* */
void __exit squeue_driver_exit(void)
{
  int i;
  /* Release the major number */
  unregister_chrdev_region((my_dev_number),4);

  for(i = 0; i < NUM_DEVICE; i++)
  {
    /* Destroy device */
    device_destroy (my_dev_class, MKDEV(MAJOR(my_dev_number), i));
    cdev_del(&my_devp[i]->cdev);
    kfree(my_devp[i]);
  }

  /* Destroy driver_class */
  class_destroy(my_dev_class);

  printk("Driver removed.\n");
}



module_init (squeue_driver_init);
module_exit (squeue_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Uma Kulkarni");
MODULE_DESCRIPTION("ESP Assignment 1- Fall 14");

// REFERENCES:
// http://www.opensourceforu.com/tag/linux-device-drivers-series/page/2/
// https://www.youtube.com/user/SolidusCode
