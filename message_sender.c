#include <errno.h> 
#include "message_slot.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/fs.h>

void error(char *msg);
size_t len_msg(char* msg);

int main (int argc, char** argv)
{
    if(argc !=4)
    {
        error(strerror(errno));
    }
    int fd = open(argv[1], O_WRONLY);
    if(fd < 0)
    {
        error(strerror(errno));
    }
    long channel_id = strtoul(argv[2], NULL, 0);
    if(ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0)
    {
        error(strerror(errno));
    }
    size_t len = len_msg(argv[3]);
    if(len == 0)
    {
        error(strerror(errno));
    }
    if(write(fd, argv[3], len)<=0)
    {
        error(strerror(errno));
    }
    if(close(fd)<0)
    {
        error(strerror(errno));
    }
    exit(0);
}

size_t len_msg(char* msg)
{
    size_t len = 0;
    while(msg[len] != '\0')
    {
        ++len;
    }
    return len;
}

void error(char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(1);
}