#ifndef INCLUDE_MAIL_BOX_
#define INCLUDE_MAIL_BOX_

#include <mthread.h>

#define MAX_MBOX_LENGTH (64)
#define MAX_MBOX_NUM 16

// TODO: please define mailbox_t;
// mailbox_t is just an id of kernel's mail box.
typedef struct mailbox
{
    int id; 
} mailbox_t;

mailbox_t mbox_open(char *name);
void mbox_close(mailbox_t mailbox);
void mbox_send(mailbox_t mailbox, void *msg, int msg_length);
void mbox_recv(mailbox_t mailbox, void *msg, int msg_length);

#endif
