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

//kernel's mail box
typedef enum {
    MBOX_OPEN,
    MBOX_CLOSE,
} mailbox_status_t;
typedef struct mailbox_k
{
    char name[25];                   //name of mailbox
    char msg[MAX_MBOX_LENGTH];       //content of message
    int index;                       //ptr of msg_buf
    int visited;                     //count when the box is used
    mailbox_status_t status;
    mthread_cond_t empty;
    mthread_cond_t full;
} mailbox_k_t;
extern mailbox_k_t mailbox_k[MAX_MBOX_NUM];

mailbox_t mbox_open(char *name);
void mbox_close(mailbox_t mailbox);
void mbox_send(mailbox_t mailbox, void *msg, int msg_length);
void mbox_recv(mailbox_t mailbox, void *msg, int msg_length);

#endif
