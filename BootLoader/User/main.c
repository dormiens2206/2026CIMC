#include "HeaderFiles.h"

#define TEAM_ID_STR   "2026413929"
//#define BOOT_UPGRADE_BAUDRATE   115200U

extern __IO uint32_t g_sys_ms;

static void Boot_ShowOLED(void)
{
    OLED_Clear();
    OLED_ShowString(0, 0,  (u8 *)TEAM_ID_STR, 16);
    OLED_ShowString(0, 16, (u8 *)"Bootloader", 16);
    OLED_Refresh();
}

static void Boot_SendStringWait(const char *str)
{
    USART485_SendString(str);

    while(!USART485_IsSendFinish())
    {
    }

    /*
     * 给上位机一点解析时间，避免下一行紧贴上一行。
     */
    delay_1ms(20);
}

static void Boot_JumpAppOrBlink(void)
{
    if(Boot_Check_App_Valid(APPLICATION_FLASH_BASE) == BOOT_OK)
    {
        Boot_Jump_To_Application(APPLICATION_FLASH_BASE);
    }

    while(1)
    {
        LED1_TOGGLE();
        delay_1ms(300);
    }
}

static void Boot_NormalStart(void)
{
    uint32_t start_ms;

    /*
     * 赛题要求：没有升级请求时，Bootloader 不做串口输出，
     * 延时 5 秒后直接跳 APP。
     */
    start_ms = g_sys_ms;

    while((g_sys_ms - start_ms) < 5000U)
    {
        LED1_TOGGLE();
        delay_1ms(200);
    }

    /*
     * 复位后从这里跳转到刚刚升级进去的官方 APP。
     */
    if(Boot_Check_App_Valid(APPLICATION_FLASH_BASE) == BOOT_OK)
    {
        Boot_Jump_To_Application(APPLICATION_FLASH_BASE);
    }

    while(1)
    {
        LED1_TOGGLE();
        delay_1ms(300);
    }
}


static void Boot_UpdateStart(void)
{
    uint32_t start_ms;
    uint32_t fw_ready_ms;
    uint8_t print_stage;

		static const uint32_t print_time_ms[4] = {0U, 3000U, 6000U, 9000U};
		static const char *print_str[4] =
		{
				"wait for start Application(10s)......\r\n",
				"wait for start Application(7s)......\r\n",
				"wait for start Application(4s)......\r\n",
				"wait for start Application(1s)......\r\n"
		};

		USART485_Init_Baud(Boot_Get_Update_Baudrate());

		Boot_ShowOLED();

		delay_1ms(300);

		Boot_SendStringWait("using command to interrupt start Application\r\n");
		Boot_SendStringWait("wait for start Application(10s)......\r\n");

		start_ms = g_sys_ms;
		fw_ready_ms = 0U;

		print_stage = 1U;

    while(1)
    {
        Protocol_Task();

				if(Bootloader_IsUpgradeDone())
				{
						/*
						 * 0503 已经执行完，官方固件已经搬到 APP 区。
						 * 直接软复位，让 Bootloader 重新从干净状态启动，再跳 APP。
						 */
						delay_1ms(500);

						Boot_Set_Update_Flag(0U);

						NVIC_SystemReset();
				}

        /*
         * 如果 0x0502 已经收完正确固件，就重新给 0x0503 一个等待窗口。
         */
        if((Bootloader_IsFirmwareReady() != 0U) && (fw_ready_ms == 0U))
        {
            fw_ready_ms = g_sys_ms;
        }

        if(Bootloader_IsFirmwareReady() == 0U)
        {
            if(print_stage < 4U)
            {
                if((g_sys_ms - start_ms) >= print_time_ms[print_stage])
                {
                    Boot_SendStringWait(print_str[print_stage]);
                    print_stage++;
                }
            }

            /*
             * 10 秒内没有收到准备传输固件命令，跳 APP。
             */
            if((g_sys_ms - start_ms) >= 10000U)
            {
                break;
            }
        }
        else
        {
            /*
             * 已经收到正确固件，但迟迟没有 0x0503，也不要永久卡死。
             */
            if((g_sys_ms - fw_ready_ms) >= 10000U)
            {
                break;
            }
        }
    }

    Boot_JumpAppOrBlink();
}

int main(void)
{
    Boot_HW_Init();

    OLED_Init();
    Boot_ShowOLED();

    /*
     * APP 收到 0x0501 后，会写 RTC_BKP1 升级标志并复位。
     * Bootloader 看到标志后进入 10 秒升级等待流程。
     */
    if(Boot_Get_Update_Flag() == BOOT_UPDATE_FLAG_VALUE)
    {
        Boot_Set_Update_Flag(0U);
        Boot_UpdateStart();
    }
    else
    {
        Boot_NormalStart();
    }

    while(1)
    {
    }
}
