#include "KEY.h"

Key_Event key_event_new = KEY_NONE;

static uint16_t key_on_cnt[5] = {0};
static const uint32_t key_list[5] = {KEY_WAKUP_PIN, KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN};
static const uint32_t key_port[5]  = {GPIOA, GPIOA, GPIOA, GPIOA, GPIOA};

static const bit_status key_bit[5] = {SET, RESET, RESET, RESET, RESET};

void KEY_Init(void)
{
    rcu_periph_clock_enable(KEY_RCU);
    rcu_periph_clock_enable(KEY_WAKUP_RCU);
    rcu_periph_clock_enable(RCU_SYSCFG);  //使能系统配置控制器时钟

    gpio_mode_set(KEY_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
                  KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN);
    
    // WKUP 下拉：未按下为低，按下为高
    gpio_mode_set(KEY_WAKUP_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, KEY_WAKUP_PIN);

    // 配置 WKUP 外部中断，用于深睡眠唤醒（按下上升沿）
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_RISING); 
    exti_interrupt_flag_clear(EXTI_0);
 
    nvic_irq_enable(EXTI0_IRQn, 2, 0);
}

/*
  在主循环中轮询
  按下计数到 100：长按（1s）
  按下计数 3~99 后松开：短按
 */
void KEY_Scan(void)
{
    for (int i = 0; i < 5; i++) 
    {
        if (gpio_input_bit_get(key_port[i], key_list[i]) == key_bit[i]) 
        {
            // 防溢出保护：最大计数到 150 就不加了
            if (key_on_cnt[i] < 150) 
            {
                key_on_cnt[i]++;
            }
            if (key_on_cnt[i] == 100) 
            {
                key_event_new = (Key_Event)(WKUP_LONG + i * 2);
            }
        } 
        else 
        {
            if (key_on_cnt[i] > 2 && key_on_cnt[i] < 100)
            {
                key_event_new = (Key_Event)(WKUP_SHORT + i * 2);
            }
            key_on_cnt[i] = 0;
        }
    }
}

//主循环取走事件后清零，避免重复触发 
Key_Event Key_Get_Event(void)
{
    Key_Event temp = key_event_new;
    key_event_new = KEY_NONE;
    return temp;
}
