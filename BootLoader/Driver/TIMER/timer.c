#include "timer.h"

void RS485_Timer_Init(void)
{

    timer_parameter_struct timer_struct;

    rcu_periph_clock_enable(RCU_TIMER5);
    timer_deinit(TIMER5);

    timer_struct.prescaler = 200 - 1;                   //预分频值
    timer_struct.period = 5000 - 1;                     //自动重装载值
    timer_struct.clockdivision = TIMER_CKDIV_DIV1;      //时钟分频，设置为不分频
    timer_struct.counterdirection = TIMER_COUNTER_UP;   //向上计数模式
    timer_struct.alignedmode = TIMER_COUNTER_EDGE;      //边缘对齐模式
    timer_struct.repetitioncounter = 0;         
    timer_init(TIMER5, &timer_struct);

    //清除更新中断标志
    timer_flag_clear(TIMER5, TIMER_FLAG_UP);

    //使能更新中断
    timer_interrupt_enable(TIMER5, TIMER_INT_UP);

    //配置中断优先级
    nvic_irq_enable(TIMER5_DAC_IRQn, 1, 0);

}

void DAC_Timer_Init(void)
{
    timer_parameter_struct timer_struct;

    rcu_periph_clock_enable(RCU_TIMER4);
    timer_deinit(TIMER4);
}
