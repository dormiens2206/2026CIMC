#include "Function.h"
#include "app_modbus_map.h"
#include "app_param.h"
#include "app_sample.h"
#include "app_storage.h"
#include "gd30ad3344.h"
#include "mb.h"
#include "project_config.h"
#include "spi_port.h"
#include "KEY.h"

extern __IO uint32_t g_sys_ms;
SystemRXMode_t  current_mode = MODE_ASCII;

void System_Init(void)
{
    switch (current_mode)
    {
        case MODE_ASCII:
        {
                
            System_Hardware_Init();
            RTC_Init();

            spi_flash_init();
            uint32_t flash_id = spi_flash_read_id();

            Param_Load(); // 加载参数

            App_ParamInit();
            Sample_Init();

            App_ModbusInit();
            App_Storage_Start();
            /* USART485_Init 第一次在硬件初始化里按默认 19200 打开；
            * 参数加载后，再按 Flash 中保存的波特率重配一次，保证重启后波特率持久化。
            */
            
            DAC_Set_Raw(DAC0_CHANNEL, g_system_params.dac_raw);
            
            USART485_Init();

            //PT100_Init();

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
        break;

        case MODE_WORK:
        {
            System_Hardware_Init();
            RTC_Init();

            spi_flash_init();
            Param_Load(); 

            App_ParamInit();
            Sample_Init();

            App_ModbusInit();
            App_Storage_Start();

            DAC_Set_Raw(DAC0_CHANNEL, g_system_params.dac_raw);

            if(eMBInit(MB_RTU,
               g_system_params.modbus_address,
               0U,
               Param_BaudCodeToBaudrate(
                   g_system_params.baud_code),
               App_ModbusParityToMB(g_system_params.parity)) == MB_ENOERR)
            {
                (void)eMBEnable();
            }

            LED_Stat = 0;
            LED_STATE();

            OLED_Clear();
            OLED_ShowString(
                0,
                0,
                (uint8_t *)"2026413929",
                16);

            OLED_ShowString(
                0,
                16,
                (uint8_t *)"IDLE",
                16);

            OLED_Refresh();
        }
        break;

        default:
            current_mode = MODE_ASCII;
            break;
    }


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

    nvic_irq_enable(
        SDIO_IRQn,
        3,
        0);
}


void Init_LED_Stat(void)
{
    LED_Stat = 0;
    LED_STATE();
}


void System_Task(void)
{
    switch(current_mode)
    {
        case MODE_ASCII:
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
        break;
        
        case MODE_WORK:
        {
            static uint32_t last_sample_ms = 0U;

            uint32_t now_ms;
            uint16_t sample_period;

            now_ms = g_sys_ms;

            sample_period =
                g_system_params.sample_period_ms;

            if(sample_period == 0U)
            {
                sample_period = 1U;
            }

            (void)eMBPoll();

            App_ModbusMap_Apply();
            App_ModbusReconfigureTask();

            // 采样任务
            if((uint32_t)(now_ms - last_sample_ms)
                >= sample_period)
            {
                last_sample_ms = now_ms;
                Sample_Task();
            }

            //TF卡存储任务
            App_StorageTask();

            /* 更新Modbus寄存器中的采样结果 */
            App_ModbusMap_Update();

            // CH2 断线警报指示灯：外部电流通道断线时点亮
            if(g_sample_data.current_break != 0U)
            {
                LED3_ON();
            }
            else
            {
                LED3_OFF();
            }

            App_AutoReportTask(); // 自动上报
            App_SleepTask();      // 睡眠唤醒
            App_ResetTask();      // 重启复位
            App_BootloaderTask(); // bootloader升级

            Alarm_CheckTask();   // 告警检测，包含告警记录和主动告警上报
            LED1_Task(); // APP 状态灯 1s 闪烁
        }
        break;
        
            default:
            current_mode = MODE_ASCII;
            break;
    }


}

void UsrFunction(void)
{
    Key_Event key = Key_Get_Event();

    if(key == KEY1_ON)
    {
        if(current_mode != MODE_ASCII)
        {
            (void)eMBDisable();     // 禁用Modbus
            current_mode = MODE_ASCII;
            USART485_Init();
        }
    }
    else if(key == KEY2_ON)
    {
        if(current_mode != MODE_WORK)
        {
            // 重新初始化Modbus
            if(eMBInit(MB_RTU,
               g_system_params.modbus_address,
               0U,
               Param_BaudCodeToBaudrate(
                   g_system_params.baud_code),
               App_ModbusParityToMB(g_system_params.parity)) == MB_ENOERR)
            {
                (void)eMBEnable();
                current_mode = MODE_WORK;
            }
        }
    }
    else if(key == KEY3_ON)
    {
        // 把用户定值导出到 TF 卡的 Config.ini
        (void)App_Storage_SaveConfig();
    }
}
