#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include "gd32f4xx.h"
#include "gd32f4xx_libopt.h"
#include "memory.h"
#include <stdint.h>
#include <string.h>

/* 使用赛题官方 Flash 分区 */
#define BOOTLOADER_FLASH_BASE      BOOTLOADER_START_ADDR
#define BOOTLOADER_FLASH_SIZE      BOOTLOADER_SIZE

#define APPLICATION_FLASH_BASE     APP_START_ADDR
#define APPLICATION_FLASH_END      APP_END_ADDR
#define APPLICATION_FLASH_SIZE     APP_SIZE

#define MCU_FLASH_BASE             FLASH_BASE_ADDR
#define MCU_FLASH_END              0x08080000U

/* 升级标志定义，后面 OTA 再用 */
#define BOOT_UPDATE_FLAG_VALUE     0xA55A5AA5U
#define BOOT_UPDATE_FLAG_REG       RTC_BKP1

#define BOOT_UPDATE_BAUD_REG       RTC_BKP2

typedef enum {
    BOOT_OK = 0U,
    BOOT_ERROR = 1U
} Boot_Status;

void Boot_HW_Init(void);

uint32_t Boot_Get_Update_Flag(void);
void Boot_Set_Update_Flag(uint32_t flag);
void Boot_Request_Update(void);

uint32_t Boot_Get_App_Stack(uint32_t app_addr);
uint32_t Boot_Get_App_Reset_Handler(uint32_t app_addr);
Boot_Status Boot_Check_App_Valid(uint32_t app_addr);
void Boot_Jump_To_Application(uint32_t app_addr);

Boot_Status Boot_Erase_App_Area(void);
Boot_Status Boot_Write_App(uint32_t offset, const uint8_t *data, uint32_t len);

uint32_t Boot_Get_Update_Baudrate(void);
void Boot_Set_Update_Baudrate(uint32_t baudrate);

#define BOOT_FW_MAGIC           0x5AA5C33CU
#define BOOT_FW_MAGIC_SIZE      4U

Boot_Status Boot_Erase_Temp_Area(void);
Boot_Status Boot_Write_Temp(uint32_t offset, const uint8_t *data, uint32_t len);
Boot_Status Boot_Check_Temp_Firmware(uint32_t fw_size);
Boot_Status Boot_Copy_Temp_To_App(uint32_t fw_size);



#endif
