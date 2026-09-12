/*
Bootloader请求
系统命令处理
DAC设置
阈值设置
*/
#include "app_task.h"
#include "sample_task.h"

extern __IO uint32_t g_sys_ms;

static uint8_t auto_report_enable = 0;  //自动上报使能标志
static uint32_t last_report_time = 0;     //上次自动上报的系统时间戳，单位毫秒
static uint16_t report_device_id = 0;   //自动上报时使用的设备ID
static uint16_t report_cmd = 0;         //自动上报时使用的命令号

static uint8_t reset_request = 0;
static uint32_t reset_request_time = 0;

static uint8_t bootloader_request = 0;
static uint32_t bootloader_request_time = 0;

#define BOOT_UPGRADE_MAGIC       0xA55A5AA5U
#define BOOT_UPGRADE_FLAG_REG    RTC_BKP1
#define BOOT_UPGRADE_BAUD_REG    RTC_BKP2

static uint8_t sleep_request = 0;     //睡眠请求标志，1表示有睡眠请求，0表示没有

//获取自动上报周期，单位毫秒
static uint32_t App_GetReportPeriod(void)
{
    switch(g_system_params.report_interval)
    {
        case REPORT_INTERVAL_1S:
            return 1000; //1秒
        case REPORT_INTERVAL_3S:
            return 3000; //3秒
        case REPORT_INTERVAL_5S:
            return 5000; //5秒
        default:
            return 1000; //默认1秒
    }
}


//发送自动上报帧的内部函数
static void App_SendAutoReportFrame(uint16_t device_id, uint16_t cmd)
{
    uint8_t payload[16];
    uint32_t timestamp;
    float ch0_value, ch1_value, ch2_value;

    //获取当前时间戳
    timestamp = RTC_GetUnixTime();

    ch0_value = Sample_Getch0();
    ch1_value = Sample_Getch1();
    ch2_value = Sample_Getch2();


    //构建payload：4字节时间戳 + 4字节CH0 + 4字节CH1 + 4字节CH2
    payload[0] = (timestamp >> 24) & 0xFF;
    payload[1] = (timestamp >> 16) & 0xFF;
    payload[2] = (timestamp >> 8) & 0xFF;
    payload[3] = timestamp & 0xFF;

    Protocol_Float2BigEnd(&ch0_value, &payload[4]);
    Protocol_Float2BigEnd(&ch1_value, &payload[8]);
    Protocol_Float2BigEnd(&ch2_value, &payload[12]);
    
    Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, cmd, payload, 16);

}

//开始自动上报
void App_StartAutoReport(uint16_t device_id, uint16_t cmd)
{
	auto_report_enable = 1;
	last_report_time = g_sys_ms;
	report_device_id = device_id;
	report_cmd = cmd;

	LED2_ON();   //自动采集灯LED2常亮

	OLED_Clear();
	OLED_ShowString(0, 0,  (uint8_t *)"2026413929", 16);
	OLED_ShowString(0, 16, (uint8_t *)"AutoSample", 16);
	OLED_Refresh();

	App_SendAutoReportFrame(device_id, cmd);        //回复数据

}

//停止自动上报
void App_StopAutoReport(uint16_t device_id, uint16_t cmd)
{
	auto_report_enable = 0;

	LED2_OFF();  //自动采集灯LED2熄灭

	OLED_Clear();
	OLED_ShowString(0, 0,  (uint8_t *)"2026413929", 16);
	OLED_ShowString(0, 16, (uint8_t *)"IDLE", 16);
	OLED_Refresh();

	Protocol_SendOK(device_id, cmd);
}

//自动上报调度函数
void App_AutoReportTask(void)
{
    uint32_t period_ms;

    if(!auto_report_enable)
    {
        return;
    }

    period_ms = App_GetReportPeriod();

    if((g_sys_ms - last_report_time) >= period_ms)
    {
        last_report_time = g_sys_ms;
        App_SendAutoReportFrame(report_device_id, report_cmd);
    }
}

//判断是否正在自动上报
uint8_t App_AutoReporting(void)
{
    return auto_report_enable;
}
//睡眠请求函数
void App_RequestSleep(uint16_t device_id, uint16_t cmd)
{
    Protocol_SendOK(device_id, cmd);

    sleep_request = 1;
}
//睡眠调度函数
void App_SleepTask(void)
{
    if(!sleep_request)
    {
        return;
    }

    /*
     * 必须先等 OK 帧发完。
     * 不然刚切到睡眠，485 还没发完，官方上位机就收不到 OK。
     */
    if(!USART485_IsSendFinish())
    {
        return;
    }

    sleep_request = 0;

    
     //RTC 闹钟 10s 后自动唤醒。
 
    RTC_SetWakeup_10s();

    
     //进入深度睡眠。这里会停在 WFI，直到 RTC_WKUP_IRQHandler 把它叫醒。
    
    PMU_Enter_DeepSleep();

    
    //醒来后关掉 wakeup timer，避免后面继续周期性中断。
    
    RTC_StopWakeup();

    
    //赛题要求这里是纯字符串，不封协议帧。
    
    USART485_Send((uint8_t *)"instrument wakeup", strlen("instrument wakeup"));
}

//重启请求函数
void App_ResetRequest(uint16_t device_id, uint16_t cmd)
{
    Protocol_SendOK(device_id, cmd);

    reset_request = 1;          //重启请求标志设为1
    reset_request_time = g_sys_ms;
}
//重启调度函数
void App_ResetTask(void)
{
    if(!reset_request)
    {
        return;
    }

    // 等待串口发送完成，确保上位机能收到 OK 帧
    if(!USART485_IsSendFinish())
    {
        return;
    }

    if((g_sys_ms - reset_request_time) < 100)
    {
        return;
    }

    reset_request = 0;

    NVIC_SystemReset();
}

static void BootFlag_UpdateSet(void)
{
    uint32_t boot_baudrate;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /*
     * APP 当前通信波特率来自 g_system_params.baud_code。
     * Param_BaudCodeToBaudrate 会把 0x13 转成 19200，
     * 把 0x14 转成 115200。
     */
    boot_baudrate = Param_BaudCodeToBaudrate(g_system_params.baud_code);

    /*
     * BKP1：告诉 Bootloader 这是升级启动，不是普通上电。
     * BKP2：告诉 Bootloader 继续使用当前波特率。
     */
    BOOT_UPGRADE_FLAG_REG = BOOT_UPGRADE_MAGIC;
    BOOT_UPGRADE_BAUD_REG = boot_baudrate;
}

void App_RequestBootloader(uint16_t device_id, uint16_t cmd)
{
    Protocol_SendOK(device_id, cmd);

    bootloader_request = 1;
    bootloader_request_time = g_sys_ms;
}

void App_BootloaderTask(void)
{
    if(!bootloader_request)
    {
        return;
    }

    if(!USART485_IsSendFinish())
    {
        return;
    }

    if((g_sys_ms - bootloader_request_time) < 100)
    {
        return;
    }

    bootloader_request = 0;

    BootFlag_UpdateSet();

    NVIC_SystemReset();
}
