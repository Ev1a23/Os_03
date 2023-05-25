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

node* minor_lst;
//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  node* head;
  node* temp;
  node* new_node;
  node* prev;
  int minor = iminor(inode);
  if(minor_lst == NULL)
  {
    //need to build the LL
    head = (node*)kmalloc(sizeof(node), GFP_KERNEL);
    if(head == NULL)
    {
      return -ENOMSG;
    }
    head->minor = minor;
    head->next = NULL;
    head->channels = NULL;
    minor_lst = head;
    return SUCCESS;
  }
  else
  {
    temp = minor_lst;
    while(temp!= NULL)
    {
      if(temp->minor == minor)
      {
        return SUCCESS;
      }
      prev = temp;
      temp = temp->next;
    }
    //need to add to LL
    new_node = (node*)kmalloc(sizeof(node), GFP_KERNEL);
    new_node->minor = minor;
    prev -> next = new_node;
    new_node->next = NULL;
    new_node->channels = NULL;
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
  void *p = file -> private_data;
  channel* cnl;
  ssize_t i;
  int res;
  char* tmp;
  if(p == NULL)
  {
    return -EINVAL;
  }
  cnl = (channel*)p;
  if(cnl->message == NULL)
  {
    return -EWOULDBLOCK;
  }
  if(length<cnl ->message_len)
  {
    return -ENOSPC;
  }
  if(buffer == NULL)
  {
    return -EINVAL;
  }
  tmp =kmalloc(BUF_LEN*sizeof(char),GFP_KERNEL);
  if(tmp == NULL)
  {
    return -ENOMSG;
  }
  for(i = 0; i<BUF_LEN&& i<cnl->message_len; i++)
  {
    res = get_user(tmp[i], &buffer[i]);
    if(res < 0)
    {
      kfree(tmp);
      return -EINVAL;
    }
  }
  kfree(tmp);
  for(i = 0; i<BUF_LEN && i<cnl->message_len; i++)
  {
    res = put_user(((char*)cnl->message)[i], &buffer[i]);
    if(res < 0)
    {
      return -EINVAL;
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
  channel* cnl;
  char* msg;
  void *p;
  ssize_t i;
  int res;
  p = file ->private_data;
  if(p == NULL)
  {
    return -EINVAL;
  }
  if(length > BUF_LEN || length == 0)
  {
    return -EMSGSIZE;
  }
  if(buffer == NULL)
  {
    return -EINVAL;
  }
  msg = (char*)kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
  if(msg == NULL)
  {
    return -ENOMEM;
  }
  cnl = (channel*)p;
  for(i = 0; i<length; i++)
  {
    res = get_user(msg[i], &buffer[i]);
    if(res<0)
    {
      kfree(msg);
      return i;
    }
  }
  kfree(cnl->message);
  cnl->message_len = length;
  cnl->message = msg;
  return length;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  int minor;
  node* temp;
  channel* cnl;
  channel* prev;
  if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)
  {
    return -EINVAL;
  }
  minor = iminor(file->f_inode);
  temp = minor_lst;
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
  cnl = temp ->channels;
  if(cnl == NULL)
  {
    cnl = (channel*)kmalloc(sizeof(channel), GFP_KERNEL);
    if(cnl == NULL)
    {
      return -ENOMSG;
    }
    cnl->channel = ioctl_param;
    cnl->message = (void*)NULL;
    cnl->message_len = 0;
    cnl->next = NULL;
    file -> private_data = (void*)cnl;
    temp->channels = cnl;
    return 1;
  }
  while(cnl != NULL)
  {
    if(cnl->channel == ioctl_param)
    {
      file -> private_data = (void*)cnl;
      return 1;
    }
    prev = cnl;
    cnl = cnl->next;
  }
  cnl = (channel*)kmalloc(sizeof(channel), GFP_KERNEL);
  if(cnl == NULL)
  {
    return -ENOMSG;
  }
  cnl->channel = ioctl_param;
  cnl->message = (void*)NULL;
  cnl->message_len = 0;
  cnl->next = NULL;
  file -> private_data = (void*)cnl;
  prev->next = cnl;
  return 1;

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
  printk("msg: In simple init");

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 ) {
    printk("%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }
  return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
  node* prev;
  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
  //now we need to free memory
  while(minor_lst != NULL)
  {
    channel* nxt = minor_lst->channels;
    channel* tmp;
    while(nxt!=NULL)
    {
      if(nxt -> message != NULL)
      {
        kfree(nxt->message);
      }
      tmp = nxt;
      nxt = nxt->next;
      kfree(tmp);
    }
    prev = minor_lst;
    minor_lst = minor_lst->next;
    kfree(prev);
  }
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
