/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
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

#include <common.h>
#include <os/irq.h>
#include <os/mm.h>
#include <os/sched.h>
#include <screen.h>
#include <sbi.h>
#include <stdio.h>
#include <os/time.h>
#include <os/syscall.h>
#include <os/futex.h>
#include <test.h>
 
#include <csr.h>
#include <os/lock.h>

extern void ret_from_exception();
extern void printk_task1(void);
extern void __global_pointer$();
mutex_lock_t binsem[NUM_MAX_SEM];

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    /* TODO: initialization registers
     * note: sp, gp, ra, sepc, sstatus
     * gp should be __global_pointer$
     * To run the task in user mode, you should set corresponding bits of sstatus(SPP, SPIE, etc.).
     * set sp to simulate return from switch_to
     * TODO: you should prepare a stack, and push some values to simulate a pcb context.
     */
    pt_regs->regs[2] = user_stack;  //sp
    pt_regs->regs[3] = __global_pointer$;                                                   //gp
    pt_regs->regs[1] = entry_point;                                                         //ra
    pt_regs->sepc = entry_point;      
    pt_regs->sstatus = SR_SPIE; //user_process:SPP==0;kernel_process:SPP=1 ??? 

    switchto_context_t *sw_regs = (switchto_context_t *)(kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t));
    sw_regs->regs[0] = (reg_t)&ret_from_exception;                                          //ra
    sw_regs->regs[1] = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);  //sp
}

//task 1
static void init_pcb()
{
     /* initialize all of your pcb and add them into ready_queue
     * TODO:初始化每一个pcb中的内容，并将进程放入就绪队列中
     */  
    pcb_t pcb[NUM_MAX_TASK];
    
    //pid=0的进程是内核本身，不算在pcb数组中，因此pid编号从1开始
    pid_t process_id = 1;
    int i;
    for ( i = 0; i < num_timer_tasks; i++,process_id++)
    {
        //初始化栈内容
        pcb[i].kernel_sp = allocPage(1) + PAGE_SIZE; 
        pcb[i].user_sp = allocPage(1) + PAGE_SIZE;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, timer_tasks[i]->entry_point, &pcb[i]);
        pcb[i].kernel_sp = pcb[i].kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
        //初始化pid,type,status,cursor
        pcb[i].pid = process_id;
        pcb[i].status = TASK_READY;
        pcb[i].type = timer_tasks[i]->type;
        pcb[i].cursor_x = 1;
        pcb[i].cursor_y = 1;
        pcb[i].list.priority = timer_tasks[i]->priority;
        //将pcb入就绪队列（拉链赋值list）
            list_add(&pcb[i].list, &ready_queue);
    }
    for (; i < num_timer_tasks+num_sched2_tasks; i++,process_id++)
    {
        //初始化栈内容
        pcb[i].kernel_sp = allocPage(1) + PAGE_SIZE; 
        pcb[i].user_sp = allocPage(1) + PAGE_SIZE;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, sched2_tasks[i-num_timer_tasks]->entry_point, &pcb[i]);
        pcb[i].kernel_sp = pcb[i].kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
        //初始化pid,type,status,cursor
        pcb[i].pid = process_id;
        pcb[i].status = TASK_READY;
        pcb[i].type = sched2_tasks[i-num_timer_tasks]->type;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].list.priority = sched2_tasks[i-num_timer_tasks]->priority;
        //将pcb入就绪队列（拉链赋值list）
        list_add(&pcb[i].list, &ready_queue);
    }
    for (; i < num_timer_tasks+num_sched2_tasks+num_lock2_tasks; i++,process_id++)
    {
        //初始化栈内容
        pcb[i].kernel_sp = allocPage(1) + PAGE_SIZE; 
        pcb[i].user_sp = allocPage(1) + PAGE_SIZE;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, lock2_tasks[i-num_timer_tasks-num_sched2_tasks]->entry_point, &pcb[i]);
        pcb[i].kernel_sp = pcb[i].kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
        //初始化pid,type,status,cursor
        pcb[i].pid = process_id;
        pcb[i].status = TASK_READY;
        pcb[i].type = lock2_tasks[i-num_timer_tasks-num_sched2_tasks]->type;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = 0;
        pcb[i].list.priority = lock2_tasks[i-num_timer_tasks-num_sched2_tasks]->priority;
        //将pcb入就绪队列（拉链赋值list）
        list_add_priority(&pcb[i].list, &ready_queue);
    }

    /* remember to initialize `current_running`
     * TODO:*/
    current_running = &pid0_pcb;
}

//task 3
static void init_syscall(void)
{
    // initialize system call table.
    int i;
	for(i = 0; i < NUM_SYSCALLS; i++)
		syscall[i] = &handle_other;
    syscall[SYSCALL_YIELD]        = &do_scheduler;
    syscall[SYSCALL_SLEEP]        = &do_sleep;
    syscall[SYSCALL_BINSEMGET]    = &do_binsemget;
    syscall[SYSCALL_BINSEMOP]     = &do_binsemop;
    syscall[SYSCALL_FUTEX_WAIT]   = &futex_wait;
    syscall[SYSCALL_FUTEX_WAKEUP] = &futex_wakeup;
    syscall[SYSCALL_WRITE]        = &screen_write;
    syscall[SYSCALL_READ]         = &sbi_console_getchar;
    syscall[SYSCALL_CURSOR]       = &screen_move_cursor;
    syscall[SYSCALL_REFLUSH]      = &screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = &get_timer;         
    syscall[SYSCALL_GET_TICK]     = &get_ticks;
}

// jump from bootloader.
// The beginning of everything >_< ~~~~~~~~~~~~~~
int main()
{
    // init Process Control Block (-_-!)
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n\r");

    // read CPU frequency
    time_base = sbi_read_fdt(TIMEBASE);
	
    // init futex mechanism
    init_system_futex();

    // init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n\r");

    // init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n\r");

    // fdt_print(riscv_dtb);

    // init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n\r");
    //init binsem_block
    int i;
    for(i=0;i<NUM_MAX_SEM;i++){
        do_mutex_lock_init(&binsem[i]);
    }

    // TODO:
    // Setup timer interrupt and enable all interrupt
    reset_irq_timer();
    enable_interrupt();
    
    while (1) {
        // (QAQQQQQQQQQQQ)
        // If you do non-preemptive scheduling, you need to use it
        // to surrender control do_scheduler();
        // enable_interrupt();
        // __asm__ __volatile__("wfi\n\r":::);
        //do_scheduler();
    };
    return 0;
}
