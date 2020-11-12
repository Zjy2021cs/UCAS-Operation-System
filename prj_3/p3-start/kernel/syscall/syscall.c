#include <os/syscall.h>

long (*syscall[NUM_SYSCALLS])();

//task 3:根据系统调用号选择要跳转的系统调用函数进行跳转。
void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    //防止例外返回时重复系统调用
    regs->sepc = regs->sepc + 4;
    //调用对应的系统调用函数：syscall[fn](arg1, arg2, arg3)
    //a0存放调用函数返回值， a7是系统调用类型，系统调用至多三个参数分别为a0,a1,a2
    regs->regs[10] = syscall[regs->regs[17]](regs->regs[10],       //a0
                                             regs->regs[11],       //a1
                                             regs->regs[12]);      //a2
    
}
