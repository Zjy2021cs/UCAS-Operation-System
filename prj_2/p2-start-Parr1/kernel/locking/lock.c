#include <os/lock.h>
#include <os/sched.h>
#include <atomic.h>

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO */
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO */
}

//task 2 锁的初始化
void do_mutex_lock_init(mutex_lock_t *lock)
{
    /* TODO */
    lock->lock.status = UNLOCKED;
    init_list_head(&lock->block_queue);
}

//task 2 锁的申请
void do_mutex_lock_acquire(mutex_lock_t *lock)
{
    /* TODO */
    if(lock->lock.status==LOCKED){
        current_running->status = TASK_BLOCKED;
        do_block(&current_running->list,&lock->block_queue);
    }
    else
        lock->lock.status=LOCKED;
}

//task 2 锁的释放
void do_mutex_lock_release(mutex_lock_t *lock)
{
    /* TODO */
    if(!list_empty(&lock->block_queue)){
        do_unblock(lock->block_queue.prev);
        lock->lock.status=LOCKED;
    }
    else
        lock->lock.status=UNLOCKED;
}
