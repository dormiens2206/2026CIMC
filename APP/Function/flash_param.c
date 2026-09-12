#include "flash_param.h"
#include "HeaderFiles.h"

/*
 * 这个文件只负责：
 * 1. 从外部 SPI Flash 读取系统参数
 * 2. 把系统参数保存到外部 SPI Flash
 * 3. 管理最近 10 条告警记录
 *
 * 用的都是工程里原来已经有的函数：
 * spi_flash_sector_erase()
 * spi_flash_buffer_write()
 * spi_flash_buffer_read()
 */

System_Params g_system_params;
Alarm_Store g_alarm_store;


static uint32_t Param_CalcCRC(const System_Params *p)
{
    return (uint32_t)Protocol_CRC16((uint8_t *)p, sizeof(System_Params) - sizeof(uint32_t));
}

static uint32_t Alarm_CalcCRC(const Alarm_Store *p)
{
    return (uint32_t)Protocol_CRC16((uint8_t *)p, sizeof(Alarm_Store) - sizeof(uint32_t));
}

uint8_t Param_IsBaudCodeValid(uint8_t baud_code)
{
    if(baud_code == BAUD_CODE_4800)  return 1;
    if(baud_code == BAUD_CODE_9600)  return 1;
    if(baud_code == BAUD_CODE_19200) return 1;
    if(baud_code == BAUD_CODE_115200)return 1;
    return 0;
}


uint32_t Param_BaudCodeToBaudrate(uint8_t baud_code)
{
    switch(baud_code)
    {
        case BAUD_CODE_4800:   return 4800U;
        case BAUD_CODE_9600:   return 9600U;
        case BAUD_CODE_19200:  return 19200U;
        case BAUD_CODE_115200: return 115200U;
        default:               return 19200U;
    }
}

static uint8_t Param_IsValid(const System_Params *p)
{
    if(p == 0)
    {
        return 0;
    }

    if(p->magic != PARAM_FLASH_MAGIC)
    {
        return 0;
    }

    if(p->version != PARAM_FLASH_VERSION)
    {
        return 0;
    }

    /*
     * 初赛设备 ID。
     */
    if(p->device_id == 0x0000U ||
       p->device_id == 0xFFFFU)
    {
        return 0;
    }

    /*
     * 波特率编码。
     */
    if(!Param_IsBaudCodeValid(p->baud_code))
    {
        return 0;
    }

    /*
     * 初赛原有参数。
     */
    if(p->dac_raw > 4095U)
    {
        return 0;
    }

    if(p->report_interval < REPORT_INTERVAL_1S ||
       p->report_interval > REPORT_INTERVAL_5S)
    {
        return 0;
    }

    if(p->alarm_mode != ALARM_MODE_ACTIVE &&
       p->alarm_mode != ALARM_MODE_PASSIVE)
    {
        return 0;
    }


    /*
     * =====================================================
     * 决赛新增参数检查
     * =====================================================
     */

    /*
     * Modbus RTU 从站地址：
     * 合法范围 1~247。
     */
    if(p->modbus_address == 0U ||
       p->modbus_address > 247U)
    {
        return 0;
    }

    /*
     * TF 存储开关只能是 0 / 1。
     */
    if(p->storage_enable > 1U)
    {
        return 0;
    }

    /*
     * 采样周期和存储周期不能为 0。
     */
    if(p->sample_period_ms == 0U)
    {
        return 0;
    }

    if(p->storage_period_ms == 0U)
    {
        return 0;
    }

    /* 数据位固定 8（Modbus RTU 规范），停止位 1/2，校验位 0~2 */
    if(p->databits != 8U)
    {
        return 0;
    }

    if(p->stopbits != 1U && p->stopbits != 2U) 
    {
        return 0;
    }
    
    if(p->parity > 2U)                  
    {
        return 0;
    }



    /*
     * 最后检查 CRC。
     */
    if(p->crc != Param_CalcCRC(p))
    {
        return 0;
    }

    return 1;
}

/* ===================== 参数区函数 ===================== */

void Param_Default(void)
{
    memset(&g_system_params, 0, sizeof(g_system_params));

    g_system_params.magic           = PARAM_FLASH_MAGIC;
    g_system_params.version         = PARAM_FLASH_VERSION;

    g_system_params.device_id       = DEFAULT_DEVICE_ID;
    g_system_params.baud_code       = BAUD_CODE_19200;
    g_system_params.report_interval = REPORT_INTERVAL_1S;

    g_system_params.ch0_ratio       = 1.0f;
    g_system_params.ch1_ratio       = 1.0f;
    g_system_params.ch2_ratio       = 1.0f;

	  g_system_params.dac_raw = 2048;
    /*
     * 默认阈值先给大一点。
     * 不然刚上电、还没设置阈值，就可能乱触发告警。
     */
    g_system_params.ch0_threshold   = 9999.0f;
    g_system_params.ch1_threshold   = 9999.0f;
    g_system_params.ch2_threshold   = 9999.0f;

    g_system_params.alarm_mode      = ALARM_MODE_PASSIVE;
		
				/*
		 * =====================================================
		 * 决赛 APP 默认参数
		 * =====================================================
		 */

		/* Modbus RTU 从站地址 */
		g_system_params.modbus_address = 1U;

		/* TF 存储默认开启 */
		g_system_params.storage_enable = 1U;

        g_system_params.databits        = 8U;
        g_system_params.stopbits        = 1U;
        g_system_params.parity          = 0U;        /* 0 = 无校验（8N1） */

		/* 默认采样周期 1000ms */
		g_system_params.sample_period_ms = 1000U;

		/* 默认 TF 存储周期 1000ms */
		g_system_params.storage_period_ms = 1000U;


		/*
		 * 三路工程量默认校准系数：
		 *
		 * y = K * x + B
		 */
		g_system_params.current_k = 1.0f;
		g_system_params.current_b = 0.0f;

		g_system_params.voltage1_k = 1.0f;
		g_system_params.voltage1_b = 0.0f;

		g_system_params.voltage2_k = 1.0f;
		g_system_params.voltage2_b = 0.0f;

        g_system_params.current_break_ma = 3.60f;
        g_system_params.current_recover_ma = 3.80f;

        g_system_params.crc = Param_CalcCRC(&g_system_params);
}

void Param_Save(void)
{
    g_system_params.magic   = PARAM_FLASH_MAGIC;
    g_system_params.version = PARAM_FLASH_VERSION;
    g_system_params.crc     = Param_CalcCRC(&g_system_params);

    spi_flash_sector_erase(PARAM_FLASH_ADDR);

    spi_flash_buffer_write((uint8_t *)&g_system_params,
                           PARAM_FLASH_ADDR,
                           sizeof(g_system_params));
}

void Param_Load(void)
{
    spi_flash_buffer_read((uint8_t *)&g_system_params,
                          PARAM_FLASH_ADDR,
                          sizeof(g_system_params));

    /*
     * 第一次烧录后，Flash 里通常全是 0xFF。
     * 这时 magic / crc 都不对，就写入默认参数。
     */
    if(!Param_IsValid(&g_system_params))
    {
        Param_Default();
        Param_Save();
    }

    AlarmFlash_Init();
}

void Param_ResetToDefault(void)
{
    Param_Default();
    Param_Save();
}

uint8_t Param_SetDeviceID(uint16_t id)
{
    if(id == 0x0000U || id == 0xFFFFU) return 0;
    g_system_params.device_id = id;
    Param_Save();
    return 1;
}

uint8_t Param_SetBaudCode(uint8_t baud_code)
{
    if(!Param_IsBaudCodeValid(baud_code)) return 0;
    g_system_params.baud_code = baud_code;
    Param_Save();
    return 1;
}

uint8_t Param_SetCH0Ratio(float ratio)
{
    g_system_params.ch0_ratio = ratio;
    Param_Save();

    return 1;
}

uint8_t Param_SetCH1Ratio(float ratio)
{
    g_system_params.ch1_ratio = ratio;
    Param_Save();

    return 1;
}

uint8_t Param_SetCH2Ratio(float ratio)
{
    g_system_params.ch2_ratio = ratio;
    Param_Save();

    return 1;
}

uint8_t Param_SetCH0Threshold(float threshold)
{
    g_system_params.ch0_threshold = threshold;
    Param_Save();

    return 1;
}

uint8_t Param_SetCH1Threshold(float threshold)
{
    g_system_params.ch1_threshold = threshold;
    Param_Save();

    return 1;
}

uint8_t Param_SetCH2Threshold(float threshold)
{
    g_system_params.ch2_threshold = threshold;
    Param_Save();

    return 1;
}

uint8_t Param_SetDacRaw(uint16_t raw)
{
    if(raw > 4095U)
    {
        return 0;
    }

    g_system_params.dac_raw = raw;
    Param_Save();

    return 1;
}

uint8_t Param_SetReportInterval(uint8_t interval_code)
{
    if(interval_code < REPORT_INTERVAL_1S || interval_code > REPORT_INTERVAL_5S) return 0;
    g_system_params.report_interval = interval_code;
    Param_Save();
    return 1;
}

uint8_t Param_SetAlarmMode(uint8_t mode)
{
    if(mode != ALARM_MODE_ACTIVE && mode != ALARM_MODE_PASSIVE) return 0;
    g_system_params.alarm_mode = mode;
    Param_Save();
    return 1;
}

static uint8_t Alarm_IsValid(const Alarm_Store *p)
{
    if(p == 0) return 0;
    if(p->magic != ALARM_FLASH_MAGIC) return 0;
    if(p->count > ALARM_MAX_RECORD) return 0;
    if(p->next_index >= ALARM_MAX_RECORD) return 0;
    if(p->crc != Alarm_CalcCRC(p)) return 0;
    return 1;
}

static void Alarm_Save(void)
{
    g_alarm_store.magic = ALARM_FLASH_MAGIC;
    if(g_alarm_store.count > ALARM_MAX_RECORD) g_alarm_store.count = ALARM_MAX_RECORD;
    if(g_alarm_store.next_index >= ALARM_MAX_RECORD) g_alarm_store.next_index = 0;
    g_alarm_store.crc = Alarm_CalcCRC(&g_alarm_store);

    spi_flash_sector_erase(ALARM_FLASH_ADDR);
    spi_flash_buffer_write((uint8_t *)&g_alarm_store,
                           ALARM_FLASH_ADDR,
                           sizeof(g_alarm_store));
}


void AlarmFlash_Clear(void)
{
    memset(&g_alarm_store, 0, sizeof(g_alarm_store));
    g_alarm_store.magic = ALARM_FLASH_MAGIC;
    g_alarm_store.count = 0;
    g_alarm_store.next_index = 0;
    Alarm_Save();
}

void AlarmFlash_Init(void)
{
    spi_flash_buffer_read((uint8_t *)&g_alarm_store,
                          ALARM_FLASH_ADDR,
                          sizeof(g_alarm_store));

    if(!Alarm_IsValid(&g_alarm_store))
    {
        AlarmFlash_Clear();
    }
}

//记录一条告警信息，包括通道、阈值、实际值和时间戳等
void AlarmFlash_Record(uint8_t ch, float threshold, float value)
{
    Time_t now;
    Alarm_RecordItem *item;

    if(g_alarm_store.next_index >= ALARM_MAX_RECORD)
    {
        g_alarm_store.next_index = 0;
    }

    RTC_GetTime(&now);

    item = &g_alarm_store.record[g_alarm_store.next_index];

    memset(item, 0, sizeof(Alarm_RecordItem));

    item->valid     = 1;
    item->ch        = ch;

    item->year      = now.year;
    item->month     = now.month;
    item->date      = now.date;
    item->hour      = now.hour;
    item->minute    = now.minute;
    item->second    = now.second;

    item->threshold = threshold;
    item->value     = value;

    g_alarm_store.next_index++;

    if(g_alarm_store.next_index >= ALARM_MAX_RECORD)
    {
        g_alarm_store.next_index = 0;
    }

    if(g_alarm_store.count < ALARM_MAX_RECORD)
    {
        g_alarm_store.count++;
    }

    Alarm_Save();
}

//告警记录字符串，返回字符串长度
uint16_t AlarmFlash_BuildString(uint8_t *buf, uint16_t max_len)
{
    uint16_t pos;

    pos = 0;

    if(buf == 0 || max_len == 0)
    {
        return 0;
    }

    buf[0] = '\0';

    if(g_alarm_store.count == 0)
    {
        const char empty_str[] = "empty";
        uint16_t len;

        len = (uint16_t)strlen(empty_str);

        if(len >= max_len) {
            len = max_len - 1;
        }

        memcpy(buf, empty_str, len);
        buf[len] = '\0';

        return len;
    }

    uint16_t i;
    for(i = 0; i < g_alarm_store.count; i++)
    {
        int index;
        Alarm_RecordItem *item;
        char line[96];
        int n;

        index = (int)g_alarm_store.next_index - 1 - (int)i;

        if(index < 0)
        {
            index += ALARM_MAX_RECORD;
        }

        item = &g_alarm_store.record[index];

        if(item->valid == 0)
        {
            continue;
        }

        n = sprintf(line,
                    "%04d-%02d-%02d %02d:%02d:%02d | CH%d | %.2f | %.2f\r\n",
                    item->year,
                    item->month,
                    item->date,
                    item->hour,
                    item->minute,
                    item->second,
                    item->ch,
                    item->threshold,
                    item->value);

        if(n <= 0)
        {
            continue;
        }

        if((pos + (uint16_t)n) >= max_len)
        {
            break;
        }

        memcpy(&buf[pos], line, (uint16_t)n);
        pos += (uint16_t)n;
        buf[pos] = '\0';
    }

    if(pos == 0)
    {
        const char empty_str[] = "empty";
        uint16_t len;

        len = (uint16_t)strlen(empty_str);

        if(len >= max_len) {
            len = max_len - 1;
        }

        memcpy(buf, empty_str, len);
        buf[len] = '\0';

        return len;
    }

    return pos;
}
