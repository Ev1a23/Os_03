#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

// The major device number.
// We don't rely on dynamic registration
// any more. We want ioctls to know this
// number at compile time.
//#define MAJOR_NUM 235
#define MAJOR_NUM 235
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0
#define DEVICE_FILE_NAME "message_slot"
#define MSG_SLOT_CHANNEL 3

#endif
