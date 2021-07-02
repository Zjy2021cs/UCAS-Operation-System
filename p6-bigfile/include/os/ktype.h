#ifndef KTYPE_H_
#define KTYPE_H_

#include <os/list.h>

typedef struct mthread_mutex
{
    int lock_id; 
} mthread_mutex_t; 

typedef struct mthread_barrier
{
    unsigned total_num;
    unsigned wait_num;
    list_head barrier_queue;
} mthread_barrier_t;

typedef struct mthread_cond
{
    list_head wait_queue;
} mthread_cond_t;

#define MAX_MBOX_LENGTH (64)
#define MAX_MBOX_NUM 16

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

#endif
