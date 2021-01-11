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

//P2: task 2 锁的初始化
void do_mutex_lock_init(mutex_lock_t *lock)
{
    /* TODO */
    lock->lock.status = UNLOCKED;
    init_list_head(&lock->block_queue);
}

//P2: task 2 锁的申请 
void do_mutex_lock_acquire(mutex_lock_t *lock)
{ 
    /* TODO */
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    if(lock->lock.status==LOCKED){
        do_block(&current_running[cpu_id]->list,&lock->block_queue);
    }
    else{
        lock->lock.status=LOCKED;
        current_running[cpu_id]->locks[current_running[cpu_id]->lock_num++]=lock; //P3-task1
    }
        
}

//P2: task 2 锁的释放
void do_mutex_lock_release(mutex_lock_t *lock)
{
    /* TODO */
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    int i;
    for(i=0;i<current_running[cpu_id]->lock_num;i++){
        if(current_running[cpu_id]->locks[i]==lock){
            int j=i+1;
            for(;j<current_running[cpu_id]->lock_num;j++){
                current_running[cpu_id]->locks[j-1]=current_running[cpu_id]->locks[j];
            }
            current_running[cpu_id]->lock_num--;
            break; 
        }
    }                                                        //P3-task1
    if(!list_empty(&lock->block_queue)){
        do_unblock(lock->block_queue.prev);
    }
    lock->lock.status=UNLOCKED;
    do_scheduler();
    /*P2-version
    if(!list_empty(&lock->block_queue)){
        pcb_t *unblock_pcb;
        unblock_pcb = list_entry(lock->block_queue.prev, pcb_t, list);
        unblock_pcb->locks[unblock_pcb->lock_num++]=lock;
        do_unblock(lock->block_queue.prev);
        lock->lock.status=LOCKED;
    }
    else
        lock->lock.status=UNLOCKED;*/
}
