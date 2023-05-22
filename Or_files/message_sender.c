#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/fs.h>
#include "message_slot.h"

size_t msg_size(char * msg){
    size_t i=0;
    while(msg[i]!='\0')
    {
        i++;
    }
    return i;
}

void error_handler(char *message)
{
    fprintf(stderr, "%s", message);
    exit(1);
}


int main(int argc, char ** argv)
{
    int fd;

    // ------ validate correct number of command line arguments ------------------
    if(argc!=4)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------

    unsigned long channel_id = strtoul(argv[2], NULL, 0);

    // ------ Step 1: Open the specified message slot device file ----------------
    fd = open(argv[1], O_WRONLY);
    if (fd<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------


    // ------ Step 2: Set channel id ---------------------------------------------    
    if(ioctl(fd, MSG_SLOT_CHANNEL, channel_id)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------

    size_t size=msg_size(argv[3]);
    
    // ------ Step 3: Write the specified message to the message slot file -------
    if (write(fd, argv[3], size)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------



    // ------ Step 5: Print the message to standard output -----------------------
    if (close(fd)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------
    
    exit(0);
}
