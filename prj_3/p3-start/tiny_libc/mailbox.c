#include <mailbox.h>
#include <string.h>
#include <sys/syscall.h>

mailbox_t mbox_open(char *name)
{
    mailbox_t mailbox;
    mailbox.id = invoke_syscall(SYSCALL_MBOX_OPEN, (uintptr_t)name, IGNORE, IGNORE);
    return mailbox;
}

void mbox_close(mailbox_t mailbox)
{
    invoke_syscall(SYSCALL_MBOX_CLOSE, mailbox.id, IGNORE, IGNORE);
}

void mbox_send(mailbox_t mailbox, void *msg, int msg_length)
{
    invoke_syscall(SYSCALL_MBOX_SEND, mailbox.id, (uintptr_t)msg, msg_length);
}

void mbox_recv(mailbox_t mailbox, void *msg, int msg_length)
{
    invoke_syscall(SYSCALL_MBOX_RECV, mailbox.id, (uintptr_t)msg, msg_length);
}
