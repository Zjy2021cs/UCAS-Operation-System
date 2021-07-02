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
#include <os/string.h>
#include <pgtable.h>
#include <user_programs.h>
#include <os/elf.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack_m = INIT_KERNEL_STACK_M + PAGE_SIZE;
pcb_t pid0_pcb_m = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack_m,
    .user_sp = (ptr_t)pid0_stack_m,
    .preempt_count = 0
};  //代表master核本身
const ptr_t pid0_stack_s = INIT_KERNEL_STACK_S + PAGE_SIZE;
pcb_t pid0_pcb_s = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack_s,
    .user_sp = (ptr_t)pid0_stack_s,
    .preempt_count = 0
};  //代表slave核本身

LIST_HEAD(ready_queue);
LIST_HEAD(exit_queue);
ptr_t recycle_queue = (ptr_t)&recycle_queue;      //list_head of re-free stack

/* current running task PCB */
pcb_t * volatile current_running[NR_CPUS];

void do_scheduler(void)
{
    __asm__ __volatile__("csrr x0, sscratch\n");  
    // TODO schedule
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    // Modify the current_running pointer.
    pcb_t *prev_running;
    prev_running = current_running[cpu_id];

    if(current_running[cpu_id]->status!=TASK_BLOCKED && current_running[cpu_id]->status!=TASK_EXITED){
        current_running[cpu_id]->status=TASK_READY;
        if(current_running[cpu_id]->pid!=0){
            list_add(&current_running[cpu_id]->list,&ready_queue);
        }
    }
    
    if(list_empty(&ready_queue)){
        if(cpu_id==0){
            current_running[cpu_id] = &pid0_pcb_m;
        }else{
            current_running[cpu_id] = &pid0_pcb_s;
        }
    }else{
        pcb_t *tail_pcb = list_entry(ready_queue.next, pcb_t, list);
        while(!list_empty(&ready_queue)){        //P3-task5? 
            pcb_t *ready_pcb = list_entry(ready_queue.prev, pcb_t, list);
            list_del(ready_queue.prev);
            if((ready_pcb->mask==3) || (ready_pcb->mask==cpu_id+1)){
                current_running[cpu_id] = ready_pcb;
                break;
            }else{
                list_add(&ready_pcb->list,&ready_queue);
                if(ready_pcb==tail_pcb){
                   if(cpu_id==0){
                        current_running[cpu_id] = &pid0_pcb_m;
                    }else{
                        current_running[cpu_id] = &pid0_pcb_s;
                    }
                    break; 
                }
            }
        }
    }
    
    current_running[cpu_id]->status=TASK_RUNNING;
    //P4-task2
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, (current_running[cpu_id]->pgdir-0xffffffc000000000)/4096);
    local_flush_tlb_all();

    // restore the current_runnint's cursor_x and cursor_y
    vt100_move_cursor(current_running[cpu_id]->cursor_x,
                      current_running[cpu_id]->cursor_y);
    // TODO: switch_to current_running
    switch_to(prev_running, current_running[cpu_id]);
}
 
//将调用该方法的进程挂起到全局timers队列，当睡眠时间达到后再由调度器从timers队列将其加入到就绪队列中继续运行
void do_sleep(uint32_t sleep_time)
{
    // TODO: sleep(seconds)
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    // note: you can assume: 1 second = `timebase` ticks
    // 1. block the current_running
    current_running[cpu_id]->status=TASK_BLOCKED;
    // 2. create a timer which calls `do_unblock` when timeout, <time.h>
    timer_create((TimerCallback)(&do_unblock), &current_running[cpu_id]->list, sleep_time*time_base);
    // 3. reschedule because the current_running is blocked.
    do_scheduler();
}

//add the node into block_queue
void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: block the pcb task into the block queue
    pcb_t *block_pcb;
    block_pcb = list_entry(pcb_node, pcb_t, list);
    block_pcb->status = TASK_BLOCKED;   
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

//P2-task4 for lock in user state--------------------------------------------------------------------------------------
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
    /**(stack_base-8) = recycle_queue;
    recycle_queue = (ptr_t)stack_base;*/
}
/* reuse the free-stack space */
ptr_t reuse(){
    long *new_stack_base;
    if(recycle_queue==(ptr_t)&recycle_queue){
        return 0;
    }else{
        new_stack_base = (long *)recycle_queue;
        recycle_queue = *(new_stack_base-8);
    }
    return (ptr_t)new_stack_base;
}

//create a process with given info, return the pid
pid_t do_spawn(task_info_t *task, void* arg, spawn_mode_t mode){
    pcb_t *new_pcb;
    if (!list_empty(&exit_queue))
    {
        new_pcb = list_entry(exit_queue.prev, pcb_t, list);
        list_del(exit_queue.prev);
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
    //init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, task->entry_point, arg, new_pcb); P4 cannot run it
    new_pcb->kernel_sp = new_pcb->kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    new_pcb->pid = process_id++;
    new_pcb->type = task->type;
    new_pcb->status = TASK_READY;
    new_pcb->mode = mode;
    new_pcb->cursor_x = 0;
    new_pcb->cursor_y = 0;
    new_pcb->lock_num = 0;
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    new_pcb->mask = current_running[cpu_id]->mask;
    list_add(&new_pcb->list, &ready_queue);
    init_list_head(&new_pcb->wait_list);
    
    return new_pcb->pid;
}
//exit from the current_running pcb, recycle the pcb/stack/lock
void do_exit(void){
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    pcb_t *exit_pcb = current_running[cpu_id];

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
    //recycle((long *)exit_pcb->user_stack_base);
    freePage(exit_pcb->user_stack_base);
    //recycle itself or by father pcb?????zombie/auto!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /* 回收pcb */
    list_add(&exit_pcb->list, &exit_queue);
    
    /* 修改状态 */
    exit_pcb->status = TASK_EXITED;            //or zombie????????????????????????????????????????????????
    exit_pcb->pid = 0;
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
    //recycle((long *)killing_pcb->user_stack_base);
    freePage(killing_pcb->user_stack_base);
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /* 回收pcb */
    list_add(&killing_pcb->list, &exit_queue);
    
    /* 修改状态 */
    killing_pcb->status = TASK_EXITED;       //or zombie????????????????????????????????????????????????????????
    killing_pcb->pid = 0;

    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    if(killing_pcb==current_running[cpu_id])
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
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    do_block(&current_running[cpu_id]->list, &pcb[i].wait_list);
    return 1;
}
void do_process_show(){
    prints("[PROCESS TABLE]\n");
    int i,j;
    i=j=0;
    for(;i<process_num;i++){
        if(pcb[i].status==TASK_RUNNING){
            int run_core;
            if(&pcb[i]==current_running[0]){
                run_core=0;
            }else{
                run_core=1;
            }
            prints("[%d] PID : %d STATUS : RUNNING MASK: 0x%d on Core %d\n",j,pcb[i].pid,pcb[i].mask,run_core);
            j++;
        }else if(pcb[i].status==TASK_READY){
            prints("[%d] PID : %d STATUS : READY MASK: 0x%d\n",j,pcb[i].pid,pcb[i].mask);
            j++;
        }else if(pcb[i].status==TASK_BLOCKED){
            prints("[%d] PID : %d STATUS : BLOCKED MASK: 0x%d\n",j,pcb[i].pid,pcb[i].mask);
            j++;
        }
    }
}
pid_t do_getpid(){
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    return current_running[cpu_id]->pid;
}
//P3-task2--------------------------------------------------------------------------------------------------------------
int do_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex){
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    current_running[cpu_id]->status = TASK_BLOCKED;    
    list_add(&current_running[cpu_id]->list,&cond->wait_queue);

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
int do_barrier_wait(mthread_barrier_t *barrier){
    barrier->wait_num++;
    if(barrier->total_num==barrier->wait_num){
        while(!list_empty(&barrier->barrier_queue))
            do_unblock(barrier->barrier_queue.prev);
        barrier->wait_num=0;
    }else{
        uint64_t cpu_id;
        cpu_id = get_current_cpu_id();
        current_running[cpu_id]->status = TASK_BLOCKED;    
        list_add(&current_running[cpu_id]->list,&barrier->barrier_queue);
        do_scheduler();
    }
    return 1;
}
//P3-task3--------------------------------------------------------------------------------------------------------------
mailbox_k_t mailbox_k[MAX_MBOX_NUM]; //kernel's mail box
int do_mbox_open(char *name)
{
    int i;
    for(i=0;i<MAX_MBOX_NUM;i++){
        if(kstrcmp(name,mailbox_k[i].name)==0){
            return i;
        }
    }
    for(i=0;i<MAX_MBOX_NUM;i++){
        if(mailbox_k[i].status==MBOX_CLOSE){
            mailbox_k[i].status=MBOX_OPEN;
            int j=0;
            while(*name){
                mailbox_k[i].name[j++]=*name;
                name++;
            }
            mailbox_k[i].name[j]='\0';
            return i;
        }
    }
    prints("No mailbox is available\n");
    return -1;
}
void do_mbox_close(int mailbox_id){
    mailbox_k[mailbox_id].status = MBOX_CLOSE;
}
void do_mbox_send(int mailbox_id, void *msg, int msg_length){
    if((mailbox_k[mailbox_id].index+msg_length)>MAX_MBOX_LENGTH){   //mailbox is full
        //block the task unil box is not full
        uint64_t cpu_id;
        cpu_id = get_current_cpu_id();
        current_running[cpu_id]->status = TASK_BLOCKED;    
        list_add(&current_running[cpu_id]->list,&mailbox_k[mailbox_id].full.wait_queue);
        do_scheduler();
    }
    //put msg in mailbox
    int i;
    for(i=0;i<msg_length;i++){
        mailbox_k[mailbox_id].msg[mailbox_k[mailbox_id].index++] = ((char*)msg)[i];
    }
    do_cond_broadcast(&mailbox_k[mailbox_id].empty);     //release all tasks waitin for msg      
}
void do_mbox_recv(int mailbox_id, void *msg, int msg_length){
    if((mailbox_k[mailbox_id].index-msg_length)<0){      //mailbox is empty
        //block the task unil box is not empty
        uint64_t cpu_id;
        cpu_id = get_current_cpu_id();
        current_running[cpu_id]->status = TASK_BLOCKED;    
        list_add(&current_running[cpu_id]->list,&mailbox_k[mailbox_id].empty.wait_queue);
        do_scheduler();
    }
    //get msg from mailbox
    int i; 
    for(i=msg_length-1;i>=0;i--){
        ((char*)msg)[i] = mailbox_k[mailbox_id].msg[--mailbox_k[mailbox_id].index];
    }
    do_cond_broadcast(&mailbox_k[mailbox_id].full);     //release all tasks waitin for send msg             
}
//P3-task5-----------------------------------------------------------------------------------------------------------
void do_taskset_p(int mask, pid_t pid){
    int i;
    for(i=0; (pcb[i].pid!=pid) && i<NUM_MAX_TASK; i++);
    if (i==NUM_MAX_TASK)
    {
        return;
    }
    pcb_t *taskset_pcb = &pcb[i];
    taskset_pcb->mask = mask;
}
void do_taskset_exec(int mask, task_info_t *task, spawn_mode_t mode){
    pcb_t *new_pcb;
    if (!list_empty(&exit_queue))
    {
        new_pcb = list_entry(exit_queue.prev, pcb_t, list);
        list_del(exit_queue.prev);
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
    //init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, task->entry_point, NULL, new_pcb); P4 cannot run it
    new_pcb->kernel_sp = new_pcb->kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    new_pcb->pid = process_id++;
    new_pcb->type = task->type;
    new_pcb->status = TASK_READY;
    new_pcb->mode = mode;
    new_pcb->cursor_x = 0;
    new_pcb->cursor_y = 0;
    new_pcb->lock_num = 0;
    new_pcb->mask = mask;
    list_add(&new_pcb->list, &ready_queue);
    init_list_head(&new_pcb->wait_list);
}

//P4-task2------------------------------------------------------------------------------------------------------------
pid_t do_exec(const char* file_name, int argc, char* argv[], spawn_mode_t mode){ //argc = num_of_arg; argv=[args]
    pcb_t *new_pcb;
    if (!list_empty(&exit_queue))
    {
        new_pcb = list_entry(exit_queue.prev, pcb_t, list);
        list_del(exit_queue.prev);
    }else
    {
        new_pcb = &pcb[process_num++];
    }
    
    new_pcb->pgdir = allocPage();
    clear_pgdir(new_pcb->pgdir);
    if((new_pcb->kernel_stack_base=reuse())==0){
        new_pcb->kernel_stack_base = allocPage() + PAGE_SIZE; //a kernel virtual addr, has been mapped
    }
    new_pcb->user_stack_base = USER_STACK_ADDR;             //a user virtual addr, not mapped
    uintptr_t kva_stack = alloc_page_helper(new_pcb->user_stack_base-0x1000, new_pcb->pgdir);
    uintptr_t src_pgdir = PGDIR_PA + 0xffffffc000000000;
    share_pgtable(new_pcb->pgdir, src_pgdir);
    //load user elf file
    int i;
    for(i=0;i<4;i++){
        if(!kstrcmp(elf_files[i].file_name,file_name)){
            break;
        }
    }
    unsigned file_len = *(elf_files[i].file_length);
    user_entry_t entry_point = (user_entry_t)load_elf(elf_files[i].file_content, file_len, new_pcb->pgdir, alloc_page_helper);
    
    new_pcb->kernel_sp =  new_pcb->kernel_stack_base;
    new_pcb->user_sp = new_pcb->user_stack_base;
    uintptr_t uva_argv = 0xf0000f040;
    uintptr_t kva_argvi = kva_stack;
    uintptr_t uva_argvi = 0xf0000f000;
    kva_stack += 0x40;
    init_pcb_stack(new_pcb->kernel_sp, new_pcb->user_sp, (ptr_t)entry_point, argc, (void *)uva_argv, new_pcb);
    for(i=0;i<argc;i++){
        if(argv[i]==0){
            break;
        }
        kmemcpy((uint8_t *)kva_argvi, (uint8_t *)argv[i], kstrlen(argv[i])+1);
        *(uintptr_t *)kva_stack = uva_argvi;
        kva_stack+=8;
        kva_argvi += kstrlen(argv[i])+1;
        uva_argvi += kstrlen(argv[i])+1;
    }
    
    new_pcb->kernel_sp = new_pcb->kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    new_pcb->pid = process_id++;
    new_pcb->type = USER_PROCESS;
    new_pcb->status = TASK_READY;
    new_pcb->mode = mode;
    new_pcb->cursor_x = 0;
    new_pcb->cursor_y = 0;
    new_pcb->lock_num = 0;
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    new_pcb->mask = current_running[cpu_id]->mask;
    list_add(&new_pcb->list, &ready_queue);
    init_list_head(&new_pcb->wait_list);
    
    return new_pcb->pid;
}
void do_show_exec(){     //ls
    int i;
    for (i=0;i<4;i++){
        prints("%s\n",elf_files[i].file_name);
    }
}