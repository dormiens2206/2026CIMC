#include "PMU.h"

extern void rcu_config(void);

//低功耗睡眠初始化
void PMU_LP_Init(void)
{
    //配置时钟
    rcu_periph_clock_enable(RCU_PMU);
}

//进入浅睡眠模式
void PMU_Enter_Sleep(void)
{
    // 浅睡眠不用关主时钟，一般用于单纯等待外设中断（比如等待串口接收完毕）
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; 
    
    pmu_to_sleepmode(WFI_CMD);
    
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

//进入深睡眠模式
void PMU_Enter_DeepSleep(void)
{
    PMU_LP_Init();

    OLED_Clear();
    LED_Stat = 0;
    LED_STATE();
    OLED_ShowString(0, 0, (uint8_t *)"Entering Sleep...", 16);
    OLED_Refresh();

    exti_interrupt_flag_clear(EXTI_0);
    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    /*
     * 进入深睡眠模式，使用低功耗LDO，开启低驱动模式，使用WFI指令进入
     * 直接切断 25M 外部晶振和 PLL 倍频器的供电。系统主频瞬间从 240MHz 掉到 0
     * CPU 停止抓取任何指令，代码直接卡在这行不往下跑
     * PMU 会把芯片内部给 CPU 供电的 1.2V LDO 核心电压进一步降低，进入微功耗维持状态
     */
    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);
    PMU_Exit_LowPower();
}

//唤醒后的处理函数
void PMU_Exit_LowPower(void)
{
    rcu_config();
    systick_config();

    exti_interrupt_flag_clear(EXTI_0);
    KEY_Init();
    OLED_Init();
}

