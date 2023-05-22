// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>     /* for kmalloc */
#include "message_slot.h"

MODULE_LICENSE("GPL");

// used to prevent concurent access into the same device
static int dev_open_flag = 0;

static struct chardev_info device_info;

// The message the device will give when asked
static char the_message[BUF_LEN];

//================== DEFINE CHANNEL LINKED LIST ===========================

struct channel
{
  int channel;
  char* message = NULL;
  struct channel* next = NULL;
  ssize_t message_len;
} channel;
//================== DEFINE MINOR LINKED LIST ===========================
struct node
{
  int minor;
  struct node* next = NULL;
  channel* channels = NULL;
} node;

node* head = NULL;
//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  int minor = iminor(inode);
  if(dev_open_flag == 0)
  {
    //need to build the LL
    head = (node*)kmalloc(sizeof(node), GFP_KERNEL);
    head->minor = minor;
    head->next = NULL;
    head->channels = NULL;
    ++dev_open_flag;
    return SUCCESS;
  }
  else
  {
    int i;
    node* temp = head;
    while(temp!= NULL)
    {
      if(temp->minor == minor)
        return SUCCESS;
      temp = temp->next;
    }
    //need to add to LL
    node* new_node = (node*)kmalloc(sizeof(node));
    new_node->minor = minor;
    temp -> next = new_node;
    new_node->next = NULL;
    new_node->channels = NULL;
    ++dev_open_flag;
    return SUCCESS;
  }
}
//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
  if(file -> private_data == NULL)
  {
    return -EINVAL;
  }
  int minor = iminor(file->f_inode);
  ssize_t i;
  node* temp = head;
  while(temp!= NULL)
  {
    if(temp->minor == minor)
      break;
    temp = temp->next;
  }
  if(temp == NULL)
  {
    return -EINVAL;
  }
  channel* channels_table = temp -> channels;
  void* p = file -> private_data;
  if(*p == NULL)
  {
    return -EINVAL;
  }
  int channel = (int)*p;
  while(channels_table != NULL)
  {
    if(channels_table -> channel == channel)
      break;
    channels_table = channels_table -> next;
  }
  if(channels_table == NULL)
  {
    return -EINVAL;
  }
  if(channels_table -> len >length)
  {
    return -ENOSPC
  }
  if(channels_table -> message == NULL)
  {
    return -EWOULDBLOCK;
  }
  for(i = 0; i < channels_table -> len; ++i)
  {
    int res = put_user(channels_table -> message[i], &buffer[i]);
    if(res < 0)
    {
      return res;
    }
  }
  return i;

}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
  if(file -> private_data == NULL)
  {
    errno = EINVAL;
    return -1;
  }
  if(length > BUF_LEN || length == 0)
  {
    errno = EMSGSIZE;
    return -1;
  }
  int minor = iminor(file->f_inode);
  ssize_t i;
  node* temp = head;
  while(temp!= NULL)
  {
    if(temp->minor == minor)
      break;
    temp = temp->next;
  }
  if(temp == NULL)
  {
    errno = EINVAL;
    return -1;
  }
  channel* channels_table = temp -> channels;
  void* p = file -> private_data;
  if(*p == NULL)
  {
    errno = EINVAL;
    return -1;
  }
  int channel = (int)*p;
  channel* prev = NULL;
  while(channels_table != NULL)
  {
    prev = channels_table;
    if(channels_table -> channel == channel)
      break;
    channels_table = channels_table -> next;
  }
  if(channels_table == NULL)
  {
    //prev is the last item in the LL, and we need to add a new channel
    if(length == 0 || length>BUF_LEN)
    {
      return -EMSGSIZE;
    }
    channel* new_channel = (channel*)kmalloc(sizeof(channel));
    new_channel -> channel = channel;
    new_channel -> len = length;
    new_channel -> message = (char*)kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
    if(new_channel_msg == NULL)
    {
      return -ENOMEM;
    }
    for(i = 0; i < length; ++i)
    {
      get_user(new_channel -> message[i], &buffer[i]);
    }
    new_channel -> next = NULL;
    prev -> next = new_channel;
    return i;
  }
  else
  {
    //need to update current channel, save in channels_table
    for(i = 0; i < BUF_LEN; ++i)
    {
      if(i < length)
      {
        int res = get_user(channels_table -> message[i], &buffer[i]);
        if(res<0)
        {
          kfree(channels_table -> message);
          return res;
        }
      }
      else
      {
        channels_table -> message[i] = NULL;
      }
    }
    channel_table -> len = length;
    return length;
  }
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)//need to figure out which command code
  {
    errno = EINVAL;
    return -1;
  }
  file -> private_data = (void*)ioctl_param;
  int minor = iminor(file->f_inode);
  int i;
  node* temp = head;
  while(temp!= NULL)
  {
    if(temp->minor == minor)
      break;
    temp = temp->next;
  }
  if(temp == NULL)
  {
    return -1;
  }
  channel* channels_table = temp -> channels;
  void* p = file -> private_data;
  if(*p == NULL)
  {
    errno = EINVAL;
    return -1;
  }
  int channel = (int) *p;
  while (channels_table -> next != NULL)
  {
    if(channels_table -> channel == channel)
      return SUCCESS;
    channels_table = channels_table -> next;
  }
  //need to add to LL
  channel new = (channel*)kmalloc(sizeof(channel));
  new -> channel = channel;
  new -> message = (char*)kmalloc(sizeof(char) * BUF_LEN);
  new -> next = NULL;
  channels_table -> next = new;
  return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
  int rc = -1;
  // init dev struct
  memset( &device_info, 0, sizeof(struct chardev_info));

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 ) {
    printk( KERN_ALERT "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }
  return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
