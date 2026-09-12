#ifndef __FLASH_PARAM_H
#define __FLASH_PARAM_H

#include <stdint.h>

/*
 * 外部 SPI Flash 地址规划
 *
 * 0x000000 ~ 0x000FFF：系统参数区，占 1 个 4KB sector
 * 0x001000 ~ 0x001FFF：告警记录区，占 1 个 4KB sector
 *
 * 注意：这里用的是开发板板载外部 SPI Flash，
 * 后面 flash_param.c 会调用你工程里原有的：
 * spi_flash_sector_erase()
 * spi_flash_buffer_write()
 * spi_flash_buffer_read()
 */
#define PARAM_FLASH_ADDR        0x000000UL
#define ALARM_FLASH_ADDR        0x001000UL

#define PARAM_FLASH_MAGIC       0x50415241UL    /* 'PARA' */
#define PARAM_FLASH_VERSION     0x00010001UL

#define ALARM_FLASH_MAGIC       0x414C4D31UL    /* 'ALM1' */
#define ALARM_MAX_RECORD        10U

/* 告警模式：和赛题 0x0601 的 payload 对齐 */
#define ALARM_MODE_ACTIVE       0x01U
#define ALARM_MODE_PASSIVE      0x02U

/* 上报间隔编码：和赛题 0x0261 的 payload 对齐 */
#define REPORT_INTERVAL_1S      0x01U
#define REPORT_INTERVAL_3S      0x02U
#define REPORT_INTERVAL_5S      0x03U

typedef struct
{
    uint32_t magic;
    uint32_t version;

    uint16_t device_id;      //设备 ID
    uint8_t  baud_code;      //波特率编码，11/12/13/14
    uint8_t  report_interval;//自动上报间隔

	//CH0/CH1 变比，CH0/CH1 阈值
    float    ch0_ratio;
    float    ch1_ratio;
    float    ch2_ratio;
    float    ch0_threshold;
    float    ch1_threshold;
    float    ch2_threshold;

	uint16_t dac_raw;       // 保存 DAC0 输出值，解决重启后 CH1 回读变 0 的问题
	uint8_t  alarm_mode;
	uint8_t  reserved;
        
    uint8_t  modbus_address;
    uint8_t  storage_enable;

    uint8_t  databits;        /* 数据位：Modbus RTU 固定 8 */
    uint8_t  stopbits;        /* 停止位：1 / 2 */
    uint8_t  parity;          /* 校验位：0=无 1=奇 2=偶 */
    uint8_t  comm_reserved;   /* 对齐保留，恒 0 */

    uint16_t sample_period_ms;
    uint16_t storage_period_ms;
    uint16_t app_reserved;

    float current_k;
    float current_b;

    float voltage1_k;
    float voltage1_b;

    float voltage2_k;
    float voltage2_b;

    float current_break_ma;
    float current_recover_ma;

    uint32_t crc;
} System_Params;

typedef struct
{
    uint8_t  valid;
    uint8_t  ch;

    uint16_t year;
    uint8_t  month;
    uint8_t  date;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    uint8_t  reserved[3];

    float    threshold;
    float    value;
} Alarm_RecordItem;

typedef struct
{
    uint32_t magic;
    uint16_t count;
    uint16_t next_index;

    Alarm_RecordItem record[ALARM_MAX_RECORD];

    uint32_t crc;
} Alarm_Store;

/* 全局参数变量，后面协议层、485 初始化都会用它 */
extern System_Params g_system_params;
extern Alarm_Store g_alarm_store;

/* 参数区 */
void Param_Default(void);
void Param_Load(void);
void Param_Save(void);
void Param_ResetToDefault(void);

uint8_t Param_SetDeviceID(uint16_t id);
uint8_t Param_SetBaudCode(uint8_t baud_code);


uint8_t Param_SetCH0Ratio(float ratio);
uint8_t Param_SetCH1Ratio(float ratio);


uint8_t Param_SetCH0Threshold(float threshold);
uint8_t Param_SetCH1Threshold(float threshold);
uint8_t Param_SetCH2Threshold(float threshold);

uint8_t Param_SetReportInterval(uint8_t interval_code);
uint8_t Param_SetAlarmMode(uint8_t mode);

uint8_t  Param_IsBaudCodeValid(uint8_t baud_code);
uint32_t Param_BaudCodeToBaudrate(uint8_t baud_code);

/* 告警记录区 */
void     AlarmFlash_Init(void);
void     AlarmFlash_Record(uint8_t ch, float threshold, float value);
void     AlarmFlash_Clear(void);
uint16_t AlarmFlash_BuildString(uint8_t *buf, uint16_t max_len);

uint8_t Param_SetCH2Ratio(float ratio);
//
uint8_t Param_SetDacRaw(uint16_t raw);

#endif
