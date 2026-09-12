#ifndef __MEMORY_H
#define __MEMORY_H

#include "gd32f4xx.h"

/* Flash 总起始地址 */
#define FLASH_BASE_ADDR          0x08000000U

/* Bootloader 区：64K */
#define BOOTLOADER_START_ADDR    0x08000000U
#define BOOTLOADER_END_ADDR      0x0800FFFFU
#define BOOTLOADER_SIZE          0x00010000U

/* 参数区：4K */
#define PARAM_START_ADDR         0x08010000U
#define PARAM_END_ADDR           0x08010FFFU
#define PARAM_SIZE               0x00001000U

/* APP 区：128K */
#define APP_START_ADDR           0x08011000U
#define APP_END_ADDR             0x08030FFFU
#define APP_SIZE                 0x00020000U

/* APP 备份区：128K */
#define APP_BACKUP_START_ADDR    0x08031000U
#define APP_BACKUP_END_ADDR      0x08050FFFU
#define APP_BACKUP_SIZE          0x00020000U

/* 固件暂存区：128K */
#define FW_TEMP_START_ADDR       0x08051000U
#define FW_TEMP_END_ADDR         0x08070FFFU
#define FW_TEMP_SIZE             0x00020000U

/* SRAM 范围 */
#define SRAM_START_ADDR          0x20000000U
#define SRAM_END_ADDR            0x20040000U

#endif
