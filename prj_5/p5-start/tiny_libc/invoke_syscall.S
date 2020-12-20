#include <asm.h>

//P2-task 3：调用syscall指令发起一次系统调用
//long invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE);
ENTRY(invoke_syscall)
  mv a7,a0
  mv a0,a1
  mv a1,a2
  mv a2,a3
  mv a3,a4      //for P4
  ecall
  jr ra
ENDPROC(invoke_syscall)
