#include <message_slot.h>
#include <linux/fs.h>
#include <errno.h>

int main (int argc, char** argv)
{
    if(argc !=3)
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
    char buffer[BUF_LEN];
    int read_bytes = read(fd, buffer, BUF_LEN);
    if(read_bytes < 0)
    {
        strerror(errno);
        exit(1);
    }
    buffer[read_bytes] = '\0';
    if(write(1, buffer, read_bytes)<0)
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