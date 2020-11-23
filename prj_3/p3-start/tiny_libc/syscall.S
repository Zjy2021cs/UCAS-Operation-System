#include <asm.h>

//task 3：调用syscall指令发起一次系统调用
//long invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE);
ENTRY(invoke_syscall)
  /* TODO: */   
  mv a7, a0
  mv a0, a1
  mv a1, a2
  mv a2, a3
  /*为了格式统一，系统调用号放在a7寄存器，time在a0*/
  ecall 
  jr ra
ENDPROC(invoke_syscall)
