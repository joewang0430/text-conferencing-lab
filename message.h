#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX_NAME 128
#define MAX_DATA 1024

enum MessageType {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK,
    PRIVATE_MSG,
    PM_NAK,
    TIMEOUT
};

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

#endif // MESSAGE_H
