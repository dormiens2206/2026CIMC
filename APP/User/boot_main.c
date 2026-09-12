/**
 * Boot 工程入口模板（默认不参与 App 工程编译）
 */
#include "HeaderFiles.h"

int main(void)
{
    systick_config();
    Boot_HW_Init();
    LED_Init();

    if (Boot_Get_Update_Flag() != 0U) {
        Boot_Set_Update_Flag(0U);
    }

    if (Boot_Check_App_Valid(APPLICATION_FLASH_BASE) == BOOT_OK) {
        Boot_Jump_To_Application(APPLICATION_FLASH_BASE);
    }

    while (1) {
        LED1_TOGGLE();
        delay_1ms(500);
    }
}
