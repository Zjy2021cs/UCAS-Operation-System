#include <sys/syscall.h>
#include <stdint.h>

pid_t sys_spawn(task_info_t *info, void* arg, spawn_mode_t mode){
    return invoke_syscall(SYSCALL_SPAWN, (uintptr_t)info, (uintptr_t)arg, mode);
}

void sys_exit(void){
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE);
}

int sys_kill(pid_t pid){
    return invoke_syscall(SYSCALL_KILL, pid, IGNORE, IGNORE);
}

int sys_waitpid(pid_t pid){
    return invoke_syscall(SYSCALL_WAITPID, pid, IGNORE, IGNORE);
}

void sys_process_show(void){
    invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE);
}

void sys_screen_clear(void){
    invoke_syscall(SYSCALL_SCREEN_CLEAR, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid(){
    return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE);
}

int sys_get_char(){
    return invoke_syscall(SYSCALL_GET_CHAR, IGNORE, IGNORE, IGNORE);
}

void sys_yield()
{
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE);
}

void sys_sleep(uint32_t time)
{
    invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE);
}

int  binsemget(int key)
{
    return invoke_syscall(SYSCALL_BINSEMGET, key, IGNORE, IGNORE);
}

int  binsemop(int binsem_id, int op)
{
    return invoke_syscall(SYSCALL_BINSEMOP, binsem_id, op, IGNORE);
}

int  binsem_destroy(int binsem_id)
{
    return invoke_syscall(SYSCALL_BINSEMDESTROY, binsem_id, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
    invoke_syscall(SYSCALL_WRITE, (uintptr_t)buff, IGNORE, IGNORE);
}

void sys_reflush()
{
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE);
}

void sys_futex_wait(volatile uint64_t *val_addr, uint64_t val)
{
    invoke_syscall(SYSCALL_FUTEX_WAIT, (uintptr_t)val_addr, val, IGNORE);
}

void sys_futex_wakeup(volatile uint64_t *val_addr, int num_wakeup)
{
    invoke_syscall(SYSCALL_FUTEX_WAKEUP, (uintptr_t)val_addr, num_wakeup, IGNORE);
}

long sys_get_timebase()
{
    return invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE);
}

long sys_get_tick()
{
    return invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE);
}

int sys_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex)
{
    return invoke_syscall(SYSCALL_COND_WAIT, cond, mutex, IGNORE);
}

int sys_cond_signal(mthread_cond_t *cond)
{
    return invoke_syscall(SYSCALL_COND_SIGNAL, cond, IGNORE, IGNORE);
}
int sys_cond_broadcast(mthread_cond_t *cond)
{
    return invoke_syscall(SYSCALL_COND_BROADCAST, cond, IGNORE, IGNORE);
}