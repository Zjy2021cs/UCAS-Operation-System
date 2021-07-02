/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/lock.h>
#include <os/ktype.h>   //#include <mthread.h> <mailbox.h>
#include <os/smp.h>
#include <context.h>

#define NUM_MAX_TASK 16

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_ZOMBIE,
    TASK_EXITED,
} task_status_t;

typedef enum {
    ENTER_ZOMBIE_ON_EXIT,
    AUTO_CLEANUP_ON_EXIT,
} spawn_mode_t;

typedef enum {
    KERNEL_PROCESS,
    KERNEL_THREAD,
    USER_PROCESS,
    USER_THREAD,
} task_type_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // this must be this order!! The order is defined in regs.h
    // 栈指针(寄存器保存在栈里)
    reg_t kernel_sp;
    reg_t user_sp;

    // count the number of disable_preempt
    // enable_preempt enables CSR_SIE only when preempt_count == 0
    reg_t preempt_count;

    //for recycle the space when killed/exited
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;
    list_head wait_list;

    /* process id */
    pid_t pid;

    /* kernel/user thread/process */
    task_type_t type;

    /* BLOCK | READY | RUNNING | ZOMBIE */
    task_status_t status;
    spawn_mode_t mode;

    /* cursor position */
    int cursor_x;
    int cursor_y;
    /* timer for sleep */
    timer_t timer;
    /* locks pcb acquired */
    int lock_num;
    mutex_lock_t *locks[10];
    /* can run on witch core */
    int mask;
    /* the kernel virtual address(phisical base address+ffffffc000000) of page directory */
    uintptr_t pgdir;
} pcb_t;

/* task information, used to init PCB */
typedef struct task_info
{
    ptr_t entry_point;
    task_type_t type; 
} task_info_t;

/* ready queue to run */
extern list_head ready_queue;

/* current running task PCB */
// extern pcb_t * volatile current_running;
extern pcb_t * volatile current_running[NR_CPUS];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
// extern pcb_t kernel_pcb[NR_CPUS];
extern pcb_t pid0_pcb_m;
extern const ptr_t pid0_stack_m;
extern pcb_t pid0_pcb_s;
extern const ptr_t pid0_stack_s;

extern void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, int argc, void* argv, pcb_t *pcb);

extern void switch_to(pcb_t *prev, pcb_t *next);
extern void do_scheduler(void);

extern void do_block(list_node_t *, list_head *queue);
extern void do_unblock(list_node_t *);

extern void do_sleep(uint32_t);
//P2-task4
#define NUM_MAX_SEM  16
extern mutex_lock_t binsem[NUM_MAX_SEM];
int do_binsemget(int key);
int do_binsemop(int binsem_id, int op);
int do_binsem_destroy(int binsem_id);
//P3-task1
/* list_head of free_recycle space */
extern ptr_t recycle_queue;         
/* exit_queue to reuse pcb */
extern list_head exit_queue;
// extern pcb[num] if can reuse
extern int process_num;
extern pid_t do_spawn(task_info_t *task, void* arg, spawn_mode_t mode);
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
//P3-task2
int do_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex);
int do_cond_signal(mthread_cond_t *cond);
int do_cond_broadcast(mthread_cond_t *cond);
int do_barrier_wait(mthread_barrier_t *barrier);
//P3-task3
int do_mbox_open(char *name);
void do_mbox_close(int mailbox_id);
void do_mbox_send(int mailbox_id, void *msg, int msg_length);
void do_mbox_recv(int mailbox_id, void *msg, int msg_length);
//P3-task5
extern void do_taskset_p(int mask, pid_t pid);
extern void do_taskset_exec(int mask, task_info_t *task, spawn_mode_t mode);
//P4-task2
typedef void (*user_entry_t)(unsigned long,unsigned long,unsigned long);
extern pid_t do_exec(const char* file_name, int argc, char* argv[], spawn_mode_t mode);
extern void do_show_exec();
//P5
/* recv_queue to wait for revieved packet*/
extern list_head recv_queue;
/* send_queue to wait for sent packet*/
extern list_head send_queue;
#endif
