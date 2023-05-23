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

//================== DEFINE CHANNEL LINKED LIST ===========================

typedef struct channel
{
  int channel;
  char* message;
  struct channel* next;
  size_t message_len;
} channel;
//================== DEFINE MINOR LINKED LIST ===========================
typedef struct node
{
  int minor;
  struct node* next;
  channel* channels;
} node;

node* head = NULL;
//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  node* head;
  node* temp;
  node* new_node;
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
    temp = head;
    while(temp!= NULL)
    {
      if(temp->minor == minor)
        return SUCCESS;
      temp = temp->next;
    }
    //need to add to LL
    new_node = (node*)kmalloc(sizeof(node), GFP_KERNEL);
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
  int minor;
  channel* channels_table;
  size_t i;
  node* temp = head;
  channel* cnl;
  int res;
  void *p = file -> private_data;
  if(p == NULL)
  {
    return -EINVAL;
  }

  minor = iminor(file->f_inode);
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

  channels_table =  temp -> channels;
  cnl = (channel*)p;
  if(cnl->message == NULL)
  {
    return -EWOULDBLOCK;
  }
  while(channels_table != NULL)
  {
    if(channels_table -> channel == cnl ->channel)
      break;
    channels_table = channels_table -> next;
  }
  if(channels_table == NULL)
  {
    return -EINVAL;
  }
  if(channels_table -> message_len == 0)
  {
    return -ENOSPC;
  }
  if(channels_table -> message == NULL)
  {
    return -EWOULDBLOCK;
  }
  for(i = 0; i < BUF_LEN && i<length; ++i)
  {
    res = put_user(channels_table -> message[i], &buffer[i]);
    if(res < 0)
    {
      kfree(channels_table -> message);
      return res;
    }
  }
  channels_table -> message_len = BUF_LEN;
  if(length < BUF_LEN)
  {
    channels_table -> message_len = length;
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
  int minor;
  size_t i;
  node* temp;
  channel* channels_table;
  channel* prev;
  channel* new_channel;
  void *p =file -> private_data;
  channel* cnl;
  int res;
  if(p== NULL)
  {
    return -EINVAL;
  }
  if(length > BUF_LEN || length == 0)
  {
    return -EMSGSIZE;
  }
  minor = iminor(file->f_inode);
  temp = head;
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
  channels_table = temp -> channels;
  cnl = (channel*)p;
  if(cnl->message == NULL)
  {
    cnl->message = (char*)kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
    if(cnl->message == NULL)
    {
      return -ENOMEM;
    }
  }
  prev = NULL;
  while(channels_table != NULL)
  {
    prev = channels_table;
    if(channels_table -> channel == cnl->channel)
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
    new_channel = (channel*)kmalloc(sizeof(channel), GFP_KERNEL);
    new_channel -> channel = cnl->channel;
    new_channel -> message_len = length;
    new_channel -> message = (char*)kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
    if(new_channel-> message == NULL)
    {
      return -ENOMEM;
    }
    for(i = 0; i < length; ++i)
    {
      res = get_user(new_channel -> message[i], &buffer[i]);
      if(res < 0)
      {
        kfree(new_channel -> message);
        return res;
      }
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
        res = get_user(channels_table -> message[i], &buffer[i]);
        if(res<0)
        {
          kfree(channels_table -> message);
          return res;
        }
      }
    }
    channels_table -> message_len = length;
    return length;
  }
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  int minor;
  node* temp;
  channel* channels_table;
  channel* new;
  void* p;
  channel* cnl;
  
  if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)//need to figure out which command code
  {
    return -EINVAL;
  }
  file -> private_data = (void*)ioctl_param;
  minor = iminor(file->f_inode);
  temp = head;
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
  channels_table = temp -> channels;
  p = file -> private_data;
  if(p == NULL)
  {
    return -EINVAL;
  }
  cnl = (channel*)p;
  while (channels_table -> next != NULL)
  {
    if(channels_table -> channel == cnl->channel)
      return SUCCESS;
    channels_table = channels_table -> next;
  }
  //need to add to LL
  new = (channel*)kmalloc(sizeof(channel), GFP_KERNEL);
  new -> channel = cnl->channel;
  new -> message = (char*)kmalloc(sizeof(char) * BUF_LEN, GFP_KERNEL);
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
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
  int rc = -1;

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
