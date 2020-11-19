#include <os/list.h>
#include <os/mm.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/irq.h>
#include <screen.h>
#include <stdio.h>
#include <assert.h>
#include <sys/binsem.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
    .preempt_count = 0
};  //代表内核本身

LIST_HEAD(ready_queue);
LIST_HEAD(exit_queue);
ptr_t recycle_queue = (ptr_t)&recycle_queue;      //list_head of re-free stack

/* current running task PCB */
pcb_t * volatile current_running;

void do_scheduler(void)
{
    __asm__ __volatile__("csrr x0, sscratch\n");  
    // TODO schedule
    // Modify the current_running pointer.
    pcb_t *prev_running;
    prev_running = current_running;

    if(current_running->status!=TASK_BLOCKED && current_running->status!=TASK_EXITED){
        current_running->status=TASK_READY;
        if(current_running->pid!=0){
            list_add(&current_running->list,&ready_queue);
        }
    }
    
    if(!list_empty(&ready_queue)){
        current_running = list_entry(ready_queue.prev, pcb_t, list);
        list_del(ready_queue.prev);
    }
    current_running->status=TASK_RUNNING;

    // restore the current_runnint's cursor_x and cursor_y
    vt100_move_cursor(current_running->cursor_x,
                      current_running->cursor_y);
    screen_cursor_x = current_running->cursor_x;
    screen_cursor_y = current_running->cursor_y;
    // TODO: switch_to current_running
    switch_to(prev_running, current_running);
}
 
//将调用该方法的进程挂起到全局timers队列，当睡眠时间达到后再由调度器从timers队列将其加入到就绪队列中继续运行
void do_sleep(uint32_t sleep_time)
{
    // TODO: sleep(seconds)
    // note: you can assume: 1 second = `timebase` ticks
    // 1. block the current_running
    current_running->status=TASK_BLOCKED;
    // 2. create a timer which calls `do_unblock` when timeout, <time.h>
    timer_create((TimerCallback)(&do_unblock), &current_running->list, sleep_time*time_base);
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

//add the node into block_queue
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: block the pcb task into the block queue
    list_add(pcb_node,queue);
    do_scheduler();
}

//move the node from block_queue into ready_queue
void do_unblock(list_node_t *pcb_node)
{
    // TODO: unblock the `pcb` from the block queue
    pcb_t *unblock_pcb;
    unblock_pcb = list_entry(pcb_node, pcb_t, list);
    unblock_pcb->status = TASK_READY;               
    list_move(pcb_node,&ready_queue);
}

//for lock in user state
int do_binsemget(int key)
{
    int id = key%16;
    return id;
}

int do_binsemop(int binsem_id, int op)
{
    mutex_lock_t *lock = &binsem[binsem_id];
    if(op==BINSEM_OP_LOCK){
        do_mutex_lock_acquire(lock);
    }else if(op==BINSEM_OP_UNLOCK){
        do_mutex_lock_release(lock);
    }
    return 1;
}

int do_binsem_destroy(int binsem_id)
{
    mutex_lock_t *lock = &binsem[binsem_id];
    /*while(!list_empty(&lock->block_queue)){
        do_unblock(lock->block_queue.prev);
    }
    lock->lock.status=UNLOCKED;*/
    if(list_empty(&lock->block_queue)){
        return 1;
    }else{
        return 0;
    }
}
//P3-task1--------------------------------------------------------------------------------------------------------------
/* 回收内存 */
void recycle(long *stack_base){
    *stack_base = recycle_queue;
    recycle_queue = (ptr_t)stack_base;
}
/* reuse the free-stack space */
ptr_t reuse(){
    long *new_stack_base;
    if(recycle_queue==(ptr_t)&recycle_queue){
        return 0;
    }else{
        new_stack_base = (long *)recycle_queue;
        recycle_queue = *new_stack_base;
    }
    return (ptr_t)new_stack_base;
}

//create a process with given info, return the pid
pid_t do_spawn(task_info_t *task, void* arg, spawn_mode_t mode){
    pcb_t *new_pcb;
    if (!list_empty(&exit_queue))
    {
        new_pcb = list_entry(exit_queue.prev, pcb_t, list);
    }else
    {
        new_pcb = &pcb[process_num++];
    }
    
    if((new_pcb->kernel_stack_base=reuse())==0){
        new_pcb->kernel_stack_base = allocPage(1) + PAGE_SIZE;
    }
    if((new_pcb->user_stack_base=reuse())==0){
        new_pcb->user_stack_base = allocPage(1) + PAGE_SIZE;
    }
    new_pcb->kernel_sp =  new_pcb->kernel_stack_base;
    new_pcb->user_sp = new_pcb->user_stack_base;
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, task->entry_point, arg, new_pcb);
    new_pcb->kernel_sp = new_pcb->kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    new_pcb->pid = process_id++;
    new_pcb->type = task->type;
    new_pcb->status = TASK_READY;
    new_pcb->mode = mode;
    new_pcb->cursor_x = 1;
    new_pcb->cursor_y = 1;
    new_pcb->lock_num = 0;
    list_add(&new_pcb->list, &ready_queue);
    init_list_head(&new_pcb->wait_list);
    
    return new_pcb->pid;
}

//exit from the current_running pcb, recycle the pcb/stack/lock
void do_exit(void){
    pcb_t *exit_pcb = current_running;

    /* 释放wait队列 */
    while(!list_empty(&exit_pcb->wait_list)){
        pcb_t *wait_pcb;
        wait_pcb = list_entry(exit_pcb->wait_list.prev, pcb_t, list);
        if(wait_pcb->status!=TASK_EXITED){
            do_unblock(exit_pcb->wait_list.prev);
        }
    }

    /* 释放锁 */ 
    int i=exit_pcb->lock_num;
    while(i){
        i--;
        do_mutex_lock_release(exit_pcb->locks[i]);
    }
    
    /* 回收内存资源 */
    recycle((long *)exit_pcb->kernel_stack_base);
    recycle((long *)exit_pcb->user_stack_base);
    //recycle itself or by father pcb?????zombie/auto!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /* 回收pcb */
    list_add(&exit_pcb->list, &exit_queue);
    
    /* 修改状态 */
    exit_pcb->status = TASK_EXITED;            //or zombie????????????????????????????????????????????????

    do_scheduler();
}

//kill process[pid], recycle the pcb/stack/lock, get away from queues
int do_kill(pid_t pid){
    int i;
    for(i=0; (pcb[i].pid!=pid) && i<NUM_MAX_TASK; i++);
    if (i==NUM_MAX_TASK)
    {
        return 0;
    }
    pcb_t *killing_pcb = &pcb[i];
    
    /* 移出所在的队列 */
    list_del(&killing_pcb->list);         //就绪队列，block队列
    list_del(&(killing_pcb->timer.list)); //timers队列

    /* 释放wait队列 */
    while(!list_empty(&killing_pcb->wait_list)){
        pcb_t *wait_pcb;
        wait_pcb = list_entry(killing_pcb->wait_list.prev, pcb_t, list);
        if(wait_pcb->status!=TASK_EXITED){
            do_unblock(killing_pcb->wait_list.prev);
        }
    }                               
    /* 释放锁 */
    i = killing_pcb->lock_num;
    while(i){
        i--;
        do_mutex_lock_release(killing_pcb->locks[i]);
    }
    
    /* 回收内存资源 */
    recycle((long *)killing_pcb->kernel_stack_base);
    recycle((long *)killing_pcb->user_stack_base);
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /* 回收pcb */
    list_add(&killing_pcb->list, &exit_queue);
    
    /* 修改状态 */
    killing_pcb->status = TASK_EXITED;       //or zombie????????????????????????????????????????????????????????
    
    if(killing_pcb==current_running)
        do_scheduler();

    return 1;
}

//current_running task should wait until process[pid] finished
int do_waitpid(pid_t pid){
    int i;
    for(i=0; (pcb[i].pid!=pid) && i<NUM_MAX_TASK; i++);
    if (i==NUM_MAX_TASK)
    {
        return 0;
    }
    current_running->status = TASK_BLOCKED;
    do_block(&current_running->list, &pcb[i].wait_list);
    return 1;
}

void do_process_show(){
    prints("[PROCESS TABLE]\n");
    int i,j;
    i=j=0;
    for(;i<process_num;i++){
        if(pcb[i].status==TASK_RUNNING){
            prints("[%d] PID : %d STATUS : RUNNING\n",j,pcb[i].pid);
            j++;
        }else if(pcb[i].status==TASK_READY){
            prints("[%d] PID : %d STATUS : READY\n",j,pcb[i].pid);
            j++;
        }else if(pcb[i].status==TASK_BLOCKED){
            prints("[%d] PID : %d STATUS : BLOCKED\n",j,pcb[i].pid);
            j++;
        }
    }
}

pid_t do_getpid(){
    return current_running->pid;
}
//P3-task2--------------------------------------------------------------------------------------------------------------
int do_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex){
    current_running->status = TASK_BLOCKED;    
    list_add(&current_running->list,&cond->wait_queue);
    do_binsemop(mutex->lock_id, BINSEM_OP_UNLOCK);
    do_scheduler();
    do_binsemop(mutex->lock_id, BINSEM_OP_LOCK);
    return 1;
}
int do_cond_signal(mthread_cond_t *cond){
    if(!list_empty(&cond->wait_queue)){
        do_unblock(cond->wait_queue.prev);
    }
    return 1;
}
int do_cond_broadcast(mthread_cond_t *cond){
    while(!list_empty(&cond->wait_queue)){
        do_unblock(cond->wait_queue.prev);
    }
    return 1;
}