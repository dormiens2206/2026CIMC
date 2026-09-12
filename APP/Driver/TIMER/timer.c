#include "timer.h"

void RS485_Timer_Init(uint32_t timeout_us)
{
    timer_parameter_struct timer_struct;

    rcu_periph_clock_enable(RCU_TIMER6);
    timer_deinit(TIMER6);

    /* TIMER6时钟为120MHz，120分频后每个计数为1us。 */
    timer_struct.prescaler = 120U - 1U;
    timer_struct.period = timeout_us - 1;                     //自动重装载值
    timer_struct.clockdivision = TIMER_CKDIV_DIV1;      //时钟分频，设置为不分频
    timer_struct.counterdirection = TIMER_COUNTER_UP;   //向上计数模式
    timer_struct.alignedmode = TIMER_COUNTER_EDGE;      //边缘对齐模式
    timer_struct.repetitioncounter = 0;         
    timer_init(TIMER6, &timer_struct);

    //清除更新中断标志
    timer_flag_clear(TIMER6, TIMER_FLAG_UP);

    //使能更新中断
    timer_interrupt_enable(TIMER6, TIMER_INT_UP);

    //配置中断优先级
    nvic_irq_enable(TIMER6_IRQn, 1, 0);

}

void DAC_Timer_Init(void)
{
    timer_parameter_struct timer_struct;

    rcu_periph_clock_enable(RCU_TIMER4);
    timer_deinit(TIMER4);
}
