#include "Function.h"
#include "gd30ad3344.h"
#include "spi_port.h"
#include "app_task.h"
#include "app_task.h"

// 作为 APP 总入口
/*
系统初始化
主循环任务调度
调用协议处理
调用自动上报任务
调用 LED/OLED 刷新
调用告警检测
*/

void System_Init(void)
{
    System_Hardware_Init();
    RTC_Init();

    spi_flash_init();
    uint32_t flash_id = spi_flash_read_id();

    Param_Load(); // 加载参数

    /* USART485_Init 第一次在硬件初始化里按默认 19200 打开；
     * 参数加载后，再按 Flash 中保存的波特率重配一次，保证重启后波特率持久化。
     */
	
	  DAC_Set_Raw(DAC0_CHANNEL, g_system_params.dac_raw);
	
    USART485_Init();

    PT100_Init();

    // 初始化LED状态
    LED_Stat = 0;
    LED_STATE();

    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)"2026413929", 16);
    OLED_ShowString(0, 16, (uint8_t *)"IDLE", 16);
    OLED_Refresh();

    // APP 上电/复位后主动心跳，用于 A-03 等待重启后心跳
    Protocol_SendHeartBeat(g_system_params.device_id);
}

void System_Hardware_Init(void)
{
    systick_config();

    LED_Init();
    KEY_Init();
    OLED_Init();
    ADC_Init();
    DAC_Init();

    PMU_LP_Init();
}

void Init_LED_Stat(void)
{
    LED_Stat = 0;
    LED_STATE();
}

void System_Task(void)
{
    static uint8_t rx_buf[600];
    uint16_t rx_len;

    rx_len = USART485_Read(rx_buf, sizeof(rx_buf));
    if (rx_len > 0)
    {
        Protocol_Process(rx_buf, rx_len);
    }
    App_AutoReportTask(); // 自动上报
    App_SleepTask();      // 睡眠唤醒
    App_ResetTask();      // 重启复位
    App_BootloaderTask(); // bootloader升级

    Alarm_CheckTask();   // 告警检测，包含告警记录和主动告警上报
    LED1_Task(); // APP 状态灯 1s 闪烁
}

void UsrFunction(void)
{
}
