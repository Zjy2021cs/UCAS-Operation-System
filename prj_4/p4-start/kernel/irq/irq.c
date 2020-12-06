#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <stdio.h>
#include <assert.h>
#include <sbi.h>
#include <screen.h>

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
}

//P2: task 3根据scause确定例外种类，调用不同的例外处理函数进行处理 
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    // TODO interrupt handler.
    // call corresponding handler by the value of `cause`
    if(cause < 0x8000000000000000){
        exc_table[regs->scause](regs,0,cause);
    }else{
        irq_table[regs->scause-0x8000000000000000](regs,1,cause);
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
    for ( i = 0; i < EXCC_COUNT; i++)
    {
        exc_table[i] = &handle_other;
    }
    exc_table[EXCC_SYSCALL] = &handle_syscall;
    setup_exception(); //part 1 don't need
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    // Output more debug information
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
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lx\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("stval: 0x%lx cause: %lx\n\r",
           stval, cause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    // printk("mhartid: 0x%lx\n\r", get_current_cpu_id());

    uintptr_t fp = regs->regs[8], sp = regs->regs[2];
    printk("[Backtrace]\n\r");
    printk("  addr: %lx sp: %lx fp: %lx\n\r", regs->regs[1] - 4, sp, fp);
    // while (fp < USER_STACK_ADDR && fp > USER_STACK_ADDR - PAGE_SIZE) {
    while (fp > 0x10000) {
        uintptr_t prev_ra = *(uintptr_t*)(fp-8);
        uintptr_t prev_fp = *(uintptr_t*)(fp-16);

        printk("  addr: %lx sp: %lx fp: %lx\n\r", prev_ra - 4, fp, prev_fp);

        fp = prev_fp;
    }

    assert(0);
}
