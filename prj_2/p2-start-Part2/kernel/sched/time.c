#include <os/time.h>
#include <os/mm.h>
#include <os/irq.h>
#include <type.h>

LIST_HEAD(timers);

uint64_t time_elapsed = 0;
uint32_t time_base = 0;

//创建timer结构体
void timer_create(TimerCallback func, void* parameter, uint64_t tick)
{
    disable_preempt();
    // TODO:
    current_running->timer.timeout_tick = tick+get_ticks();
    current_running->timer.callback_func = func;
    current_running->timer.parameter = parameter;
    list_add(&(current_running->timer.list),&timers);
    enable_preempt();
}

//检查每个timer对应的任务是否用完时间，若计时到tick时间就调用func函数执行
void timer_check()
{
    disable_preempt();
    /* TODO: check all timers
     *  if timeouts, call callback_func and free the timer.
     */
    uint64_t current_tick;
    list_node_t *current_node = timers.prev;
    list_node_t *next_node;
    while (current_node!=&timers)
    {
        next_node = current_node->prev;
        current_tick = get_ticks();
        timer_t *current_timer;
        current_timer = list_entry(current_node, timer_t, list);
        if (current_tick >= current_timer->timeout_tick)
        {
            current_timer->callback_func(current_timer->parameter);
            list_del(current_node);
        }
        current_node = next_node;
    }
    enable_preempt();
}

//上电到现在经过的周期数
uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

//timebase:一秒多少周期；
//当前经过了多少秒
uint64_t get_timer()
{
    __asm__ __volatile__("csrr x0, sscratch\n"); 
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}
