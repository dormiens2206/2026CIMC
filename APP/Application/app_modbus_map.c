#include "app_modbus_map.h"
#include "app_param.h"
#include "app_storage.h"
#include "app_sample.h"
#include "flash_param.h"
#include "mb.h"
#include "port.h"

uint16_t REG_INPUT_BUF[REG_INPUT_SIZE];   // 输入寄存器，只读，功能码 04
uint16_t REG_HOLD_BUF[REG_HOLD_SIZE];     // 保持寄存器，读写，功能码 03/06/16
uint8_t  REG_COILS_BUF[REG_COILS_SIZE];   // 线圈，读写，功能码 01/05/15
uint8_t  REG_DISC_BUF[REG_DISC_SIZE];     // 离散输入，只读，功能码 02

// 标记 Modbus 配置变更请求是否待处理
static uint8_t g_modbus_reconfigure_pending = 0U;  

// 将 float 转换为两个 uint16_t 寄存器
static void Modbus_PutFloat32(uint16_t *reg, float value)
{
    union
    {
        float f;
        uint32_t u_32;
    } converter;

    converter.f = value;

    reg[0] = (uint16_t)(converter.u_32 >> 16);
    reg[1] = (uint16_t)(converter.u_32 & 0xFFFF);
}

// 将两个 uint16_t 寄存器转换为 float
static float Modbus_GetFloat32(const uint16_t *reg)
{
    union
    {
        float f;
        uint32_t u_32;
    } converter;

    converter.u_32 = ((uint32_t)reg[0] << 16) | (uint32_t)reg[1];

    return converter.f;
}

// 检查 Modbus 地址和数量是否在有效范围内
static uint8_t App_ModbusRangeValid(uint16_t address, uint16_t count, uint16_t used_count)
{
    if(count == 0U || address >= used_count || count > (uint16_t)(used_count - address))
    {
        return 0U; // 无效范围
    }
    return 1U; // 有效范围

}

uint8_t App_ModbusInputAddressValid(uint16_t addr, uint16_t cnt)
{
    return App_ModbusRangeValid(addr, cnt, REG_INPUT_USED_COUNT);
}

uint8_t App_ModbusHoldingAddressValid(uint16_t addr, uint16_t cnt)
{
    return App_ModbusRangeValid(addr, cnt, REG_HOLD_USED_COUNT);
}

uint8_t App_ModbusCoilsAddressValid(uint16_t addr, uint16_t cnt)
{
    return App_ModbusRangeValid(addr, cnt, REG_COILS_USED_COUNT);
}

uint8_t App_ModbusDiscreteAddressValid(uint16_t addr, uint16_t cnt)
{
    return App_ModbusRangeValid(addr, cnt, REG_DISC_USED_COUNT);
}

// Holding 寄存器写入前校验
uint8_t App_ModbusHoldingValueValid(uint16_t index, uint16_t value)
{
    switch(index)
    {
        case MB_HOLD_YEAR:
        case MB_HOLD_MONTH:
        case MB_HOLD_DAY:
        case MB_HOLD_HOUR:
        case MB_HOLD_MINUTE:
        case MB_HOLD_SECOND:
            // RTC 时间寄存器，暂不校验
            return 1U;
        
        case MB_HOLD_COMM_ADDR:
            // Modbus 从站地址，合法范围 1~247
            return (value >= 1U && value <= 247U) ? 1U : 0U;

        case MB_HOLD_COMM_BAUD:
            // RS485 波特率寄存器，合法值为 0x11、0x12、0x13、0x14
            return (value == 0x11U || value == 0x12U || value == 0x13U || value == 0x14U) ? 1U : 0U;

        case MB_HOLD_COMM_DATABITS:
            // 数据位寄存器，合法值为 8
            return (value == 8U) ? 1U : 0U;

        case MB_HOLD_COMM_STOPBITS:
            // 停止位寄存器，合法值为 1 或 2
            return (value == 1U || value == 2U) ? 1U : 0U;

        case MB_HOLD_COMM_PARITY:
            // 校验位寄存器，合法值为 0、1、2
            return (value <= 2U) ? 1U : 0U;

        case MB_HOLD_CH0_RATIO: case MB_HOLD_CH0_RATIO+1:
        case MB_HOLD_CH1_RATIO: case MB_HOLD_CH1_RATIO+1:
        case MB_HOLD_CH2_RATIO: case MB_HOLD_CH2_RATIO+1:
        case MB_HOLD_CH0_LIMIT: case MB_HOLD_CH0_LIMIT+1:
        case MB_HOLD_CH1_LIMIT: case MB_HOLD_CH1_LIMIT+1:
        case MB_HOLD_CH2_LIMIT: case MB_HOLD_CH2_LIMIT+1:
            // 变比和阈值寄存器，暂不校验
            return 1U;
        default:
            // 其他寄存器暂不校验
            return 1U;
    }
}

// 从 Flash 恢复系统参数到 Holding 寄存器
static void App_ModbusHoldingFromParam(void)
{
    Time_t now;

    RTC_GetTime(&now);
    // 将当前时间写入 Holding 寄存器
    REG_HOLD_BUF[MB_HOLD_YEAR]   = now.year;
    REG_HOLD_BUF[MB_HOLD_MONTH]  = now.month;
    REG_HOLD_BUF[MB_HOLD_DAY]    = now.date;
    REG_HOLD_BUF[MB_HOLD_HOUR]   = now.hour;
    REG_HOLD_BUF[MB_HOLD_MINUTE] = now.minute;
    REG_HOLD_BUF[MB_HOLD_SECOND] = now.second;

    // 将系统参数写入 Holding 寄存器
    REG_HOLD_BUF[MB_HOLD_COMM_ADDR]     = g_system_params.modbus_address;
    REG_HOLD_BUF[MB_HOLD_COMM_BAUD]     = g_system_params.baud_code;
    REG_HOLD_BUF[MB_HOLD_COMM_DATABITS] = g_system_params.databits;
    REG_HOLD_BUF[MB_HOLD_COMM_STOPBITS] = g_system_params.stopbits;
    REG_HOLD_BUF[MB_HOLD_COMM_PARITY]   = g_system_params.parity;

    // 将浮点数参数写入 Holding 寄存器
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH0_RATIO], g_system_params.ch0_ratio);
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH1_RATIO], g_system_params.ch1_ratio);
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH2_RATIO], g_system_params.ch2_ratio);
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH0_LIMIT], g_system_params.ch0_threshold);
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH1_LIMIT], g_system_params.ch1_threshold);
    Modbus_PutFloat32(&REG_HOLD_BUF[MB_HOLD_CH2_LIMIT], g_system_params.ch2_threshold);
    
}

void App_ModbusInit(void)
{
    //上电：把Flash恢复出来的参数装进Holding寄存器
    App_ModbusHoldingFromParam();

    g_modbus_reconfigure_pending = 0U; // 清除配置变更请求标志

    // Coil初始状态：运行指示灯默认点亮，电流断线默认正常
    REG_COILS_BUF[MB_COIL_RUN_INDICATOR] = 1U;
    REG_COILS_BUF[MB_COIL_CURRENT_BREAK] = 0U;
}

// 周期刷新只读区：Input Coil Discrete
void App_ModbusMap_Update(void)
{
    Time_t now;
    float ch0, ch1, ch2;

    RTC_GetTime(&now);
    REG_INPUT_BUF[MB_INPUT_YEAR]   = now.year;
    REG_INPUT_BUF[MB_INPUT_MONTH]  = now.month;
    REG_INPUT_BUF[MB_INPUT_DAY]    = now.date;
    REG_INPUT_BUF[MB_INPUT_HOUR]   = now.hour;
    REG_INPUT_BUF[MB_INPUT_MINUTE] = now.minute;
    REG_INPUT_BUF[MB_INPUT_SECOND] = now.second;

    ch0 = Sample_Getch0();
    ch1 = Sample_Getch1();
    ch2 = Sample_Getch2();

    // 将采样值写入 Input 寄存器
    Modbus_PutFloat32(&REG_INPUT_BUF[MB_INPUT_CH0], ch0);
    Modbus_PutFloat32(&REG_INPUT_BUF[MB_INPUT_CH1], ch1);
    Modbus_PutFloat32(&REG_INPUT_BUF[MB_INPUT_CH2], ch2);

    // 将采样状态写入 Coil 寄存器
    REG_COILS_BUF[MB_COIL_CURRENT_BREAK] = g_sample_data.current_break;

    //Discrete Input 寄存器：设备故障、通道告警
    REG_DISC_BUF[MB_DISC_DEVICE_FAULT] = (
                    g_sample_data.current_error ||
                    g_sample_data.voltage1_error ||
                    g_sample_data.voltage2_error) ? 1U : 0U;
    
    // 通道告警状态：根据采样值和阈值判断
    REG_DISC_BUF[MB_DISC_CH0_OVER_ALARM] = (ch0 > g_system_params.ch0_threshold)? 1U : 0U;
    REG_DISC_BUF[MB_DISC_CH1_OVER_ALARM] = (ch1 > g_system_params.ch1_threshold)? 1U : 0U;
    REG_DISC_BUF[MB_DISC_CH2_OVER_ALARM] = (ch2 > g_system_params.ch2_threshold)? 1U : 0U;
}

// 检查 Modbus 配置参数是否有变更
static uint8_t App_ModbusParamChanged(const System_Params *old, const System_Params *new)
{
    if(old->modbus_address != new->modbus_address) return 1U;
    if(old->baud_code != new->baud_code) return 1U;
    if(old->databits != new->databits) return 1U;
    if(old->stopbits != new->stopbits) return 1U;
    if(old->parity != new->parity) return 1U;
    
    if(old->ch0_ratio != new->ch0_ratio) return 1U;
    if(old->ch1_ratio != new->ch1_ratio) return 1U;
    if(old->ch2_ratio != new->ch2_ratio) return 1U;
    if(old->ch0_threshold != new->ch0_threshold) return 1U;
    if(old->ch1_threshold != new->ch1_threshold) return 1U; 
    if(old->ch2_threshold != new->ch2_threshold) return 1U;

    return 0U; // 没有变更

}

//RTC时间合法性
static uint8_t App_ModbusRtcValid(const Time_t *t)
{
    if(t->year < 2000U || t->year > 2099U) return 0U;
    if(t->month < 1U || t->month > 12U) return 0U;
    if(t->date < 1U || t->date > 31U) return 0U;
    if(t->hour > 23U) return 0U;
    if(t->minute > 59U) return 0U;
    if(t->second > 59U) return 0U;

    return 1U; // RTC 时间合法
}

void App_ModbusMap_Apply(void)
{
    System_Params newp;
    const System_Params *old = &g_system_params;
    Time_t now;
    Time_t new_time;
    uint16_t addr;
    uint8_t baud, databits, stopbits,parity;

    newp = *old;

    RTC_GetTime(&now);

    new_time.year = REG_HOLD_BUF[MB_HOLD_YEAR];
    new_time.month = REG_HOLD_BUF[MB_HOLD_MONTH];
    new_time.date = REG_HOLD_BUF[MB_HOLD_DAY];
    new_time.hour = REG_HOLD_BUF[MB_HOLD_HOUR];
    new_time.minute = REG_HOLD_BUF[MB_HOLD_MINUTE];
    new_time.second = REG_HOLD_BUF[MB_HOLD_SECOND];
    new_time.week = 0U; // 周字段暂不使用

    // Holding RTC与当前时间不同，即主站写了新时间
    if(new_time.year != now.year || new_time.month != now.month || new_time.date != now.date ||
       new_time.hour != now.hour || new_time.minute != now.minute || new_time.second != now.second)
    {
        if(!App_ModbusRtcValid(&new_time))
        {
            App_ModbusHoldingFromParam();
            return;
        }
        RTC_SetTime(&new_time);
    }

    addr = REG_HOLD_BUF[MB_HOLD_COMM_ADDR];
    baud = REG_HOLD_BUF[MB_HOLD_COMM_BAUD];
    databits = REG_HOLD_BUF[MB_HOLD_COMM_DATABITS];
    stopbits = REG_HOLD_BUF[MB_HOLD_COMM_STOPBITS];
    parity = REG_HOLD_BUF[MB_HOLD_COMM_PARITY];

    if(addr == 0U || addr > 247U)
    {
        App_ModbusHoldingFromParam();
        return;
    }

    if(!Param_IsBaudCodeValid(baud))
    {
        App_ModbusHoldingFromParam();
        return;
    }

    if(databits != 8U)
    {
        App_ModbusHoldingFromParam();
        return;
    }

    if(stopbits != 1U && stopbits != 2U)
    {
        App_ModbusHoldingFromParam();
        return;
    }

    if(parity > 2U)
    {
        App_ModbusHoldingFromParam();
        return;
    }

    newp.modbus_address = addr;
    newp.baud_code = baud;
    newp.databits = databits;
    newp.stopbits = stopbits;
    newp.parity = parity;

    newp.ch0_ratio = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH0_RATIO]);
    newp.ch1_ratio = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH1_RATIO]);
    newp.ch2_ratio = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH2_RATIO]);
    newp.ch0_threshold = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH0_LIMIT]);
    newp.ch1_threshold = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH1_LIMIT]);
    newp.ch2_threshold = Modbus_GetFloat32(&REG_HOLD_BUF[MB_HOLD_CH2_LIMIT]);

    if(newp.ch0_ratio <= 0.0f || newp.ch1_ratio <= 0.0f || newp.ch2_ratio <= 0.0f)
    {
        App_ModbusHoldingFromParam();
        return;
    }

    if(App_ModbusParamChanged(old, &newp) == 0U)
    {
        return;
    }

    if(newp.modbus_address != old -> modbus_address ||
       newp.baud_code != old -> baud_code ||
       newp.databits != old -> databits ||
       newp.stopbits != old -> stopbits ||
       newp.parity != old -> parity)
    {
        g_modbus_reconfigure_pending = 1U; // 标记 Modbus 配置变更请求
    }
    g_system_params = newp;
    Param_Save();
}

void App_ModbusReconfigureTask(void)
{
    eMBParity parity;

    if(g_modbus_reconfigure_pending == 0U)
    {
        return;
    }

    if(xMBPortSerialIsTxIdle() != TRUE)
    {
        return;
    }

    if(eMBDisable() != MB_ENOERR)
    {
        return;
    }

    parity = (eMBParity)App_ModbusParityToMB(g_system_params.parity);

    extern volatile uint8_t g_mb_databits;
    extern volatile uint8_t g_mb_stopbits;
    g_mb_databits = g_system_params.databits;
    g_mb_stopbits = g_system_params.stopbits;

    if(eMBInit(MB_RTU,
        g_system_params.modbus_address,
        0U,
        Param_BaudCodeToBaudrate(g_system_params.baud_code),
        parity) != MB_ENOERR)
    {
        return;
    }

    if(eMBEnable() != MB_ENOERR)
    {
        return;
    }

    g_modbus_reconfigure_pending = 0U; // 清除配置变更请求标志
}

uint8_t App_ModbusParityToMB(uint8_t parity)
{
    if(parity > 2U) return 0U;
    return parity; // 0=无 1=奇 2=偶
}