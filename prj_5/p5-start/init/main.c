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
#include <os/ioremap.h>
#include <os/irq.h>
#include <os/mm.h>
#include <os/sched.h>
#include <screen.h>
#include <sbi.h>
#include <stdio.h>
#include <os/time.h>
#include <os/syscall.h>
#include <os/futex.h>
#include <assert.h>
 
#include <csr.h>
#include <os/lock.h>
#include <os/ktype.h>
#include <os/smp.h>

#include <pgtable.h>
#include <user_programs.h>
#include <os/elf.h>

#include <plic.h>
#include <emacps/xemacps_example.h>
#include <net.h>

extern void ret_from_exception();
extern void __global_pointer$();
pcb_t pcb[NUM_MAX_TASK];
/* global process id */ //pid=0的进程是内核本身，不算在pcb数组中，因此pid编号从1开始
pid_t process_id = 1;
/* global used process num */
int process_num = 0;
mutex_lock_t binsem[NUM_MAX_SEM];

void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point, int argc, void* argv,
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
    //pt_regs->regs[10] = (reg_t)arg;                                                        //a0
    /* P4-task3 */
    pt_regs->regs[10] = (reg_t)argc;                                                         //a0
    pt_regs->regs[11] = (reg_t)argv;                                                         //a1
    pt_regs->sepc = (reg_t)entry_point;      
    pt_regs->sstatus = SR_SPIE | SR_SUM; //user_process:SPP==0;kernel_process:SPP=1 ??? 

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
    //............alloc page dir for process
    pcb[0].pgdir = allocPage();
    clear_pgdir(pcb[0].pgdir);
    pcb[0].kernel_stack_base = allocPage() + PAGE_SIZE;   //a kernel virtual addr, has been mapped
    pcb[0].user_stack_base = USER_STACK_ADDR;             //a user virtual addr, not mapped
    //............map user_stack_page to phaddr
    uintptr_t kva_stack = alloc_page_helper(pcb[0].user_stack_base-0x1000, pcb[0].pgdir);
    //............copy kernel pgdir map to pcb[0].pgdir
    uintptr_t src_pgdir = PGDIR_PA + 0xffffffc000000000;
    share_pgtable(pcb[0].pgdir, src_pgdir);
    //load user elf file
    unsigned file_len = *(elf_files[0].file_length);
    user_entry_t entry_point = (user_entry_t)load_elf(elf_files[0].file_content, file_len, pcb[0].pgdir, alloc_page_helper);
    pcb[0].kernel_sp =  pcb[0].kernel_stack_base;
    pcb[0].user_sp = pcb[0].user_stack_base;
    init_pcb_stack(pcb[0].kernel_sp, pcb[0].user_sp, (ptr_t)entry_point, 0, NULL, &pcb[0]); 
    pcb[0].kernel_sp = pcb[0].kernel_sp -sizeof(regs_context_t) - sizeof(switchto_context_t); 
    //初始化pid,type,status,cursor
    pcb[0].pid = process_id++;
    pcb[0].type = USER_PROCESS; 
    pcb[0].status = TASK_READY;
    pcb[0].mode = AUTO_CLEANUP_ON_EXIT;
    pcb[0].cursor_x = 0;
    pcb[0].cursor_y = 0;
    pcb[0].lock_num = 0;
    pcb[0].mask = 3;
    //将pcb入就绪队列（拉链赋值list）
    list_add(&pcb[0].list, &ready_queue);
    init_list_head(&pcb[0].wait_list);
    process_num++; 

    /* remember to initialize `current_running`
     * TODO:*/
    current_running[0] = &pid0_pcb_m;
    current_running[1] = &pid0_pcb_s;
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
    syscall[SYSCALL_EXEC]           = (long int (*)())&do_exec;
    syscall[SYSCALL_SHOW_EXEC]      = (long int (*)())&do_show_exec;
    syscall[SYSCALL_GETPID]         = (long int (*)())&do_getpid;
    syscall[SYSCALL_YIELD]          = (long int (*)())&do_scheduler;
    
    syscall[SYSCALL_FUTEX_WAIT]     = (long int (*)())&futex_wait;
    syscall[SYSCALL_FUTEX_WAKEUP]   = (long int (*)())&futex_wakeup;
    syscall[SYSCALL_TASKSET_P]      = (long int (*)())&do_taskset_p;
    syscall[SYSCALL_TASKSET_EXEC]   = (long int (*)())&do_taskset_exec;

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
    syscall[SYSCALL_BINSEMDESTROY]  = (long int (*)())&do_binsem_destroy;
    syscall[SYSCALL_COND_WAIT]      = (long int (*)())&do_cond_wait;
    syscall[SYSCALL_COND_SIGNAL]    = (long int (*)())&do_cond_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long int (*)())&do_cond_broadcast;
    syscall[SYSCALL_BARRIER_WAIT]   = (long int (*)())&do_barrier_wait;
    syscall[SYSCALL_MBOX_OPEN]      = (long int (*)())&do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long int (*)())&do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long int (*)())&do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long int (*)())&do_mbox_recv;

    syscall[SYSCALL_NET_RECV]       = (long int (*)())&do_net_recv;
    syscall[SYSCALL_NET_SEND]       = (long int (*)())&do_net_send;
    syscall[SYSCALL_NET_IRQ_MODE]   = (long int (*)())&do_net_irq_mode;
}

// jump from bootloader.
// The beginning of everything >_< ~~~~~~~~~~~~~~
int main()
{
    /*uint64_t try_va = 0xffffffc04000801c;
    int try = *(int *)try_va;
    printk("try read:%x\n",try);*/
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    /* master core */
    if(cpu_id==0){
        // init Process Control Block (-_-!)
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n\r");

        // read CPU frequency
        time_base = sbi_read_fdt(TIMEBASE);

        uint32_t slcr_bade_addr = 0, ethernet_addr = 0;
        slcr_bade_addr = sbi_read_fdt(SLCR_BADE_ADDR);
        printk("[slcr] phy: 0x%x\n\r", slcr_bade_addr);

        // get_prop_u32(_dtb, "/soc/ethernet/reg", &ethernet_addr);
        ethernet_addr = sbi_read_fdt(ETHERNET_ADDR);
        printk("[ethernet] phy: 0x%x\n\r", ethernet_addr);

        uint32_t plic_addr = 0;
        // get_prop_u32(_dtb, "/soc/interrupt-controller/reg", &plic_addr);
        plic_addr = sbi_read_fdt(PLIC_ADDR);
        printk("[plic] plic: 0x%x\n\r", plic_addr);

        uint32_t nr_irqs = sbi_read_fdt(NR_IRQS);
        // get_prop_u32(_dtb, "/soc/interrupt-controller/riscv,ndev", &nr_irqs);
        printk("[plic] nr_irqs: 0x%x\n\r", nr_irqs);

        XPS_SYS_CTRL_BASEADDR =
            (uintptr_t)ioremap((uint64_t)slcr_bade_addr, NORMAL_PAGE_SIZE);
        xemacps_config.BaseAddress =
            (uintptr_t)ioremap((uint64_t)ethernet_addr, 9*NORMAL_PAGE_SIZE);
            //real:(uintptr_t)ioremap((uint64_t)ethernet_addr, NORMAL_PAGE_SIZE);
        xemacps_config.BaseAddress += 0x8000;     //for qemu
        uintptr_t _plic_addr =
            (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000*NORMAL_PAGE_SIZE);
        // XPS_SYS_CTRL_BASEADDR = slcr_bade_addr;
        // xemacps_config.BaseAddress = ethernet_addr;
        xemacps_config.DeviceId        = 0;
        xemacps_config.IsCacheCoherent = 0;

        printk(
            "[slcr_bade_addr] phy:%x virt:%lx\n\r", slcr_bade_addr,
            XPS_SYS_CTRL_BASEADDR);
        printk(
            "[ethernet_addr] phy:%x virt:%lx\n\r", ethernet_addr,
            xemacps_config.BaseAddress);
        printk("[plic_addr] phy:%x virt:%lx\n\r", plic_addr, _plic_addr);
        plic_init(_plic_addr, nr_irqs);
    
        long status = EmacPsInit(&EmacPsInstance);
        if (status != XST_SUCCESS) {
            printk("Error: initialize ethernet driver failed!\n\r");
            assert(0);
        }
	
        // init futex mechanism
        init_system_futex();

        // init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n\r");

        // init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n\r");

        // init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n\r");
        //init binsem_block
        int i;
        for(i=0;i<NUM_MAX_SEM;i++){
            do_mutex_lock_init(&binsem[i]);
        }
        //init mailbox_k
        for(i=0;i<MAX_MBOX_NUM;i++){
            mailbox_k[i].index = 0;
            mailbox_k[i].visited = 0;
            mailbox_k[i].status = MBOX_CLOSE;
            init_list_head(&mailbox_k[i].empty.wait_queue);
            init_list_head(&mailbox_k[i].full.wait_queue);
        }

        net_poll_mode = 1;
        // xemacps_example_main();
    
        /*/ Wake up slave core
        smp_init();
        lock_kernel();
        wakeup_other_hart();*/
        lock_kernel();
        //cancle temp mapping for 50200000
        uintptr_t pgdir = PGDIR_PA+8+0xffffffc000000000;
        *(PTE *)pgdir = 0;
        /*try slave core only.........
        unlock_kernel();
        while(1);*/
    }else
    {   /* slave core*/
        lock_kernel();
        setup_exception();
    }
    printk("running cpu_id=%d\n",cpu_id);
    // TODO: Setup timer interrupt and enable all interrupt
    reset_irq_timer();
    unlock_kernel();
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
