#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <stdio.h>
#include <os/smp.h>
#include <assert.h>
#include <sbi.h>
#include <screen.h>
#include <os/mm.h>
#include <emacps/xemacps_example.h>
#include <plic.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void reset_irq_timer()
{
    // TODO clock interrupt handler.
    // TODO: call following functions when task4
    screen_reflush();
    timer_check();
    uint64_t stime_value;
    stime_value = get_ticks() + time_base/200;
    sbi_set_timer(stime_value);
    // note: use sbi_set_timer
    // remember to reschedule
    do_scheduler();
}//prj_5 scheduler, time counter in here to do, emmmmmm maybe.

//P2: task 3根据scause确定例外种类，调用不同的例外处理函数进行处理 
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    // TODO interrupt handler.
    // call corresponding handler by the value of `cause`
    if(cause < 0x8000000000000000){
        exc_table[regs->scause](regs,stval,cause);
    }else{
        irq_table[regs->scause-0x8000000000000000](regs,stval,cause);
    }
}

void handle_int(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    reset_irq_timer();
}

//P2: task 4
void init_exception()
{
    /* TODO: initialize irq_table and exc_table */
    /* note: handle_int, handle_syscall, handle_other, etc.*/
    int i;
    for ( i = 0; i < IRQC_COUNT; i++)
    {
        irq_table[i] = &handle_other;
    }
    irq_table[IRQC_S_TIMER] = &handle_int;
    irq_table[IRQC_S_EXT] = &plic_handle_irq;
    for ( i = 0; i < EXCC_COUNT; i++)
    {
        exc_table[i] = &handle_other;
    }
    exc_table[EXCC_SYSCALL] = &handle_syscall;
    //exc_table[EXCC_STORE_PAGE_FAULT] = &handle_pagefault;
    setup_exception(); //part 1 don't need
}

//P4
void handle_pagefault(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    uint64_t cpu_id;
    cpu_id = get_current_cpu_id();
    uintptr_t kva;
    kva = alloc_page_helper(stval, current_running[cpu_id]->pgdir);
}

//P5 
extern uint64_t read_sip();
void handle_irq(regs_context_t *regs, uint64_t irq)
{
    // TODO: 
    // handle external irq from network device
    uint32_t ISR_reg;
    ISR_reg = XEmacPs_ReadReg(EmacPsInstance.Config.BaseAddress, XEMACPS_ISR_OFFSET);
    if(ISR_reg & XEMACPS_IXR_FRAMERX_MASK){      //complete getting a packet
        int recieved_num=0;
        while(bd_space[recieved_num]%2){
            recieved_num++;
        }
        if((bd_space[recieved_num-1]%4)==3){   //reieve enough packet
            if (!list_empty(&recv_queue)) {
                do_unblock(recv_queue.prev);
            }
        }
        //set XEMACPS_ISR_OFFSET for complete recieve
        XEmacPs_WriteReg(EmacPsInstance.Config.BaseAddress, XEMACPS_ISR_OFFSET,ISR_reg); 
        //set RXSR for complete recieve
        XEmacPs_WriteReg(EmacPsInstance.Config.BaseAddress, XEMACPS_RXSR_OFFSET,XEMACPS_RXSR_FRAMERX_MASK);
        // NOTE: remember to flush dcache
        Xil_DCacheFlushRange(0, 64);
        // let PLIC know that handle_irq has been finished
        plic_irq_eoi(irq);
    }
    if(ISR_reg & XEMACPS_IXR_TXCOMPL_MASK){      //complete sending a packet
        if (!list_empty(&send_queue)) {
            do_unblock(send_queue.prev);
        }
        //set XEMACPS_ISR_OFFSET for complete recieve/send
        XEmacPs_WriteReg(EmacPsInstance.Config.BaseAddress, XEMACPS_ISR_OFFSET,ISR_reg);  
        //set TXSR for complete recieve
        uint32_t TXSR_reg;
        TXSR_reg = XEmacPs_ReadReg(EmacPsInstance.Config.BaseAddress, XEMACPS_TXSR_OFFSET);
        XEmacPs_WriteReg(EmacPsInstance.Config.BaseAddress, XEMACPS_TXSR_OFFSET,TXSR_reg|XEMACPS_TXSR_TXCOMPL_MASK);  
        // NOTE: remember to flush dcache
        Xil_DCacheFlushRange(0, 64);
        // let PLIC know that handle_irq has been finished
        plic_irq_eoi(irq);
    }
    
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    uint64_t mhartid = get_current_cpu_id();
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx pid: %d mhartid: %d\n\r", 
           regs->sepc, current_running[mhartid]->pid, mhartid);

    // if (regs->sbadaddr != 0) {
    //     PTE* pte = va2pte(regs->sbadaddr, current_running->pgdir);
    //     if (pte != NULL) {
    //         printk("PTE : %lx %lx\n\r", (uintptr_t) pte, *pte);
    //         uintptr_t baseAddr = ((uintptr_t) pte) >> 12 << 12;
    //         for (uintptr_t addr = baseAddr; addr < baseAddr + PAGE_SIZE; addr += 8) {
    //             printk("PTE : %lx %lx %lx\n\r", addr, get_pfn(*(PTE*)addr), get_attribute(*(PTE*)addr, 0x3ff));
    //         }
    //     }
    // }

    uintptr_t fp = regs->regs[8], sp = regs->regs[2];
    printk("[Backtrace]\n\r");
    printk("  addr: %lx sp: %lx fp: %lx\n\r", regs->regs[1] - 4, sp, fp);
    // while (fp < USER_STACK_ADDR && fp > USER_STACK_ADDR - PAGE_SIZE) {
    while (fp < USER_STACK_ADDR && fp > 0x10000) {
        uintptr_t prev_ra = *(uintptr_t*)(fp-8);
        uintptr_t prev_fp = *(uintptr_t*)(fp-16);

        printk("  addr: %lx sp: %lx fp: %lx\n\r", prev_ra - 4, fp, prev_fp);

        fp = prev_fp;
    }
    assert(0);
}
