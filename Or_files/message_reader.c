#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "message_slot.h"


void error_handler(char *message)
{
    fprintf(stderr, "%s", message);
    exit(1);
}

int main(int argc, char ** argv)
{
    int fd;
    char *message;
    char *buf = (char*)malloc(BUF_LEN*sizeof(char));
  


    // ------ validate correct number of command line arguments ------------------
    if(argc!=3)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------
    
    unsigned long channel_id = strtoul(argv[2], NULL, 0);
    
    // ------ Step 1: Open the specified message slot device file ----------------
    fd = open(argv[1], O_RDONLY);
    if(fd<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------


    // ------ Step 2: Set channel id ---------------------------------------------
    if(ioctl(fd, MSG_SLOT_CHANNEL,channel_id)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------



    // ------ Step 3: Read a message from the message slot file to a buffer ------
    if(buf==NULL) 
        error_handler(strerror(errno)); //allocation failed
    int res = read(fd, buf, BUF_LEN);
    if (res<0)
        error_handler(strerror(errno)); //read failed
    message=(char*)malloc(res*sizeof(char));
    for (int i=0; i<res; i++){
	message[i]=buf[i];
    }
    free(buf);
    // ---------------------------------------------------------------------------



    // ------ Step 4: Close the device -------------------------------------------
    if(close(fd)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------



    // ------ Step 5: Print the message to standard output -----------------------
    if (write(STDOUT_FILENO, message, res)<0)
        error_handler(strerror(errno));
    // ---------------------------------------------------------------------------
    

    free(buf);
    exit(0);
}
