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

int main (int argc, char** argv)
{
    if(argc !=3)
    {
        error(strerror(errno));
    }
    int fd = open(argv[1], O_RDONLY);
    if(fd < 0)
    {
        error(strerror(errno));
    }
    long channel_id = strtoul(argv[2], NULL, 0);
    if(ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0)
    {
        error(strerror(errno));
    }
    char buffer[BUF_LEN];
    if(buffer==NULL)
    {
        error(strerror(errno));
    }
    int read_bytes = read(fd, buffer, BUF_LEN);
    if(read_bytes <=0)
    {
        error(strerror(errno));
    }
    char msg[read_bytes];
    for(int i = 0; i < read_bytes; ++i)
    {
        msg[i] = buffer[i];
    }

    if(close(fd)<0)
    {
        error(strerror(errno));
    }

    if(write(1, msg, read_bytes)<0)
    {
        error(strerror(errno));
    }
    
    exit(0);

}

void error(char *msg)
{
    fprintf(stderr, "%s", msg);
    exit(1);
}