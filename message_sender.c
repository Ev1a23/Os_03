#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <errno.h> /* error codes */
#include "message_slot.h"

int main (int argc, char** argv)
{
    if(argc !=4)
    {
        strerror(errno);
        exit(1);
    }
    int fd = open(argv[1], O_RDONLY);
    if(fd < 0)
    {
        strerror(errno);
        exit(1);
    }
    int channel_id = atoi(argv[2]);
    if(ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0)
    {
        strerror(errno);
        exit(1);
    }
    ssize_t len = len_msg(argv[3]);
    if(write(fd, argv[3], len)<0)
    {
        strerror(errno);
        exit(1);
    }
    if(close(fd)<0)
    {
        strerror(errno);
        exit(1);
    }
    exit(0);
}

ssize_t len_msg(char* msg)
{
    ssize_t len = 0;
    while(msg[len] != '\0')
    {
        ++len;
    }
    return len;
}