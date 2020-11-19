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
pcb_t pcb[NUM_MAX_TASK];
/* global process id */ //pid=0的进程是内核本身，不算在pcb数组中，因此pid编号从1开始
pid_t process_id = 1;
/* global used process num */
int process_num = 0;
mutex_lock_t binsem[NUM_MAX_SEM];

void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, void* arg,
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
    pt_regs->regs[2]  = (reg_t)user_stack;                                                   //sp
    pt_regs->regs[3]  = (reg_t)__global_pointer$;                                            //gp
    pt_regs->regs[1]  = (reg_t)entry_point;                                                  //ra
    pt_regs->regs[10] = (reg_t)arg;                                                          //a0
    pt_regs->sepc = (reg_t)entry_point;      
    pt_regs->sstatus = SR_SPIE; //user_process:SPP==0;kernel_process:SPP=1 ??? 

    switchto_context_t *sw_regs = (switchto_context_t *)(kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t));
    sw_regs->regs[0] = (reg_t)&ret_from_exception;                                          //ra
    sw_regs->regs[1] = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);  //sp
}

/* initialize all of your pcb and add them into ready_queue
 * TODO:初始化每一个pcb中的内容，并将进程放入就绪队列中
 */ 
static void init_pcb()
{
    //only init shell to spawn other tasks
    //初始化栈内容
    pcb[0].kernel_stack_base = allocPage(1) + PAGE_SIZE;
    pcb[0].user_stack_base = allocPage(1) + PAGE_SIZE;
    pcb[0].kernel_sp =  pcb[0].kernel_stack_base;
    pcb[0].user_sp = pcb[0].user_stack_base;
    init_pcb_stack(pcb[0].kernel_sp, pcb[0].user_sp, (ptr_t)&test_shell, NULL, &pcb[0]);
    pcb[0].kernel_sp = pcb[0].kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    //初始化pid,type,status,cursor
    pcb[0].pid = process_id++;
    pcb[0].type = USER_PROCESS; 
    pcb[0].status = TASK_READY;
    pcb[0].mode = AUTO_CLEANUP_ON_EXIT;
    pcb[0].cursor_x = 1;
    pcb[0].cursor_y = 1;
    pcb[0].lock_num = 0;
    //将pcb入就绪队列（拉链赋值list）
    list_add(&pcb[0].list, &ready_queue);
    init_list_head(&pcb[0].wait_list);
    process_num++;

    /* remember to initialize `current_running`
     * TODO:*/
    current_running = &pid0_pcb;
}

static void init_syscall(void)
{
    // initialize system call table.
    int i;
	for(i = 0; i < NUM_SYSCALLS; i++)
		syscall[i] = (long int (*)())&handle_other;
    syscall[SYSCALL_SPAWN]          = (long int (*)())&do_spawn;
    syscall[SYSCALL_EXIT]           = (long int (*)())&do_exit;
    syscall[SYSCALL_SLEEP]          = (long int (*)())&do_sleep;
    syscall[SYSCALL_KILL]           = (long int (*)())&do_kill;
    syscall[SYSCALL_WAITPID]        = (long int (*)())&do_waitpid;
    syscall[SYSCALL_PS]             = (long int (*)())&do_process_show;
    syscall[SYSCALL_GETPID]         = (long int (*)())&do_getpid;
    syscall[SYSCALL_YIELD]          = (long int (*)())&do_scheduler;

    syscall[SYSCALL_FUTEX_WAIT]     = (long int (*)())&futex_wait;
    syscall[SYSCALL_FUTEX_WAKEUP]   = (long int (*)())&futex_wakeup;

    syscall[SYSCALL_WRITE]          = (long int (*)())&screen_write;
    syscall[SYSCALL_READ]           = (long int (*)())&sbi_console_getchar; 
    syscall[SYSCALL_CURSOR]         = (long int (*)())&screen_move_cursor;
    syscall[SYSCALL_REFLUSH]        = (long int (*)())&screen_reflush;
    /*syscall[SYSCALL_SERIAL_READ]    = &;
    syscall[SYSCALL_SERIAL_WRITE]   = &;
    syscall[SYSCALL_READ_SHELL_BUFF]= &;*/
    syscall[SYSCALL_SCREEN_CLEAR]   = (long int (*)())&screen_clear;

    syscall[SYSCALL_GET_TIMEBASE]   = (long int (*)())&get_timer;         
    syscall[SYSCALL_GET_TICK]       = (long int (*)())&get_ticks;
    syscall[SYSCALL_GET_CHAR]       = (long int (*)())&sbi_console_getchar;
     
    syscall[SYSCALL_BINSEMGET]      = (long int (*)())&do_binsemget;
    syscall[SYSCALL_BINSEMOP]       = (long int (*)())&do_binsemop;
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
