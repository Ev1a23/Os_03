#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>

#include "message_slot.h"


MODULE_LICENSE("GPL");

typedef struct linked_list_node{
    unsigned int key; // key represents the minor for slot nodes and the chnl_id for channel nodes.
    void *data; // data represents the channel list for slot nodes and the message array for channel nodes.
    size_t len; // represents a channel message length. relevant only for channel nodes.
    struct linked_list_node *next;
    struct linked_list_node *prev;
} NODE;

typedef struct linked_list{
    NODE *head;
    //int length;
} LLIST;

NODE* find(LLIST *list, unsigned int v)
{ // @ret: a pointer to the node in list with key = v
    NODE *node = list->head;
    while(node!=NULL){
	if(node->key==v)
            return node;
        node=node->next;
    }
    return NULL;
}

NODE* insert(LLIST *list, unsigned int v, void *d)
{ /*@ret: a pointer to the node in 'list' with key=v on success or NULL on failure.
    @pre: there's at most one node in 'list' with key=v.
    @post: there's at most one node in 'list' with key=v. */
    NODE *h=find(list, v);
    if(h!=NULL)
    {// a node with key=v already exsits in list:
        return h;
    }
    // a node with key=v does not exsits in list:
    h=(NODE*)kmalloc(sizeof(NODE),GFP_KERNEL);
    if(h!=NULL)
    {//allocation secceeded:
        h->key=v;
        h->data=d;
	h->len=0;
        h->next=list->head;
	if(h->next!=NULL)
	        h->next->prev=h;
        list->head=h;
        h->prev=NULL;
    }
    return h;
}


void delete_channel(LLIST *list, unsigned int key){ // relevant for channel nodes only!
    NODE *node = find(list, key);
    if(node->next!=NULL && node->prev!=NULL){
        node->prev->next=node->next;
        node->next->prev=node->prev;
    }
    if(node->next==NULL){
        node->prev->next=NULL;
        node->prev=NULL;
    }
    if(node->prev==NULL){
        list->head=node->next;
        node->next=NULL;
    }
    if(node->data!=NULL)
        kfree(node->data);
    kfree(node);
    node=NULL;
}


void delete_channel_list(LLIST *cnl_list)
{
	while(cnl_list->head!=NULL){
		delete_channel(cnl_list, cnl_list->head->key);
	}
}


void delete(LLIST *list, unsigned int key){ // relevant for slot nodes only!
    NODE *node = find(list, key);
    if(node->next!=NULL && node->prev!=NULL){
        node->prev->next=node->next;
        node->next->prev=node->prev;
    }
    if(node->next==NULL){
        node->prev->next=NULL;
        node->prev=NULL;
    }
    if(node->prev==NULL){
        list->head=node->next;
        node->next=NULL;
    }
    if (node->data!=NULL)
        delete_channel_list((LLIST*)(node->data));
    kfree(node);
    node=NULL;
}


void delete_list(LLIST *list)
{
    while(list->head!=NULL){
        delete(list, list->head->key);
    }
}




LLIST *slots; // list of open message slots.

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode, struct file*  file )
{
    void *empty_list = kmalloc(sizeof(LLIST), GFP_KERNEL);
    printk("Invoking device_open(%p)\n", file);
    if(find(slots, (unsigned int)(iminor(inode))) != NULL)
	    return 0;
    if(empty_list == NULL)
        return -ENOMSG;
    ((LLIST*)empty_list)->head=NULL;
    if(insert(slots, (unsigned int)(iminor(inode)), empty_list)==NULL)
    {// add the slot to the active slots list.
        kfree(empty_list);
        return -ENOMSG;
    }
    return 0;
}


//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    ssize_t i;
    void *p = file->private_data;
    NODE *channel;
    //char *tmp;
    printk("Invoking device_read(%p)\n", file);
    //tmp=(char*)kmalloc(BUF_LEN*sizeof(char) ,GFP_KERNEL);
    //if(tmp==NULL)
//	return -ENOMSG;

    if(p==NULL)
        return -EINVAL;
    channel = (NODE*)p;

    if(channel->data==NULL)
	return -EWOULDBLOCK;

    if (length < channel->len)
            return -ENOSPC;
    
    for( i = 0; i < length && i < BUF_LEN && i < channel->len; ++i ) {
        int res = put_user(((char*)channel->data)[i], &buffer[i]);
	if(res<0)
	        return res;
    }

    // return the number of input characters used
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
    ssize_t i;
    void *p;
    NODE* channel;
    char *the_message;
    printk("Invoking device_write(%p,%ld)\n", file, length);  
    the_message=(char*)kmalloc(BUF_LEN*sizeof(char) ,GFP_KERNEL);
    if(the_message==NULL)
	return -ENOMSG;

    if (length==0 || length > BUF_LEN)
        return -EMSGSIZE; 

    p = file->private_data;
    if(p==NULL)
        return -EINVAL;

    channel = (NODE*)p;

    if (channel->data==NULL)
    {
	channel->data = kmalloc(BUF_LEN, GFP_KERNEL);
        if(channel->data==NULL)
            return -ENOMSG;
    }
    for( i = 0; i < length && i < BUF_LEN; ++i ) {
	int res = get_user(the_message[i], &buffer[i]);
        if (res < 0)
	{
	    kfree(the_message);
            return res;
	}
    }
    channel->data = the_message;
    channel -> len = length;
   
    // return the number of input characters used
    return channel->len;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    unsigned int minor;
    NODE *slot;
    NODE *channel_node;
    printk("Invoking device_ioctl(%p)\n",file);
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)
        return -EINVAL;

    minor=(unsigned int)(iminor(file->f_inode));
    slot = find(slots, minor);
    if(slot==NULL)
	;//error??

    channel_node = insert(slot->data, (unsigned int)ioctl_param, (void*)NULL);
    if(channel_node==NULL)
        return -ENOMSG;
    printk("setting private data to %p", channel_node);
    file->private_data=(void*)channel_node;
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
  // init dev struct
  //memset( &device_info, 0, sizeof(struct chardev_info) );
  //spin_lock_init( &device_info.lock );

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 ) {
    printk( KERN_ERR "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }

  printk( "Registeration is successful. ");
  printk( "If you want to talk to the device driver,\n" );
  printk( "you have to create a device file:\n" );
  printk( "mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM );
  printk( "You can echo/cat to/from the device file.\n" );
  printk( "Dont forget to rm the device file and "
          "rmmod when you're done\n" );

  slots = (LLIST*)kmalloc(sizeof(LLIST), GFP_KERNEL);
  slots->head = NULL;
  return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); //can that fail?

}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);
