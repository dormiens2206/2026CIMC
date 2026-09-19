#include "app_sample.h"
#include "project_config.h"
#include "gd30ad3344.h"
#include "spi_port.h"
#include "app_param.h"
#include <string.h>
#include <stdio.h>

#define CURRENT_ADC_FSR           4.096f
#define VOLTAGE_ADC_FSR           4.096f
#define ADC_SIGNED_FULL_SCALE     32768.0f
#define AMC_GAIN_NOMINAL          8.0f
#define VOLTAGE_DIV_RATIO         (1.0f / 50.9f)
#define DIFF_TO_SE_GAIN           0.5f
#define VOLTAGE_OFFSET_NOMINAL    1.65f
#define CURRENT_SHUNT_NOMINAL     10.0f

#define CURRENT_4MA_RAW           2560

//#define CURRENT_BREAK_MA          3.60f
//#define CURRENT_RECOVER_MA        3.80f

#define CURRENT_BREAK_CONFIRM_CNT 3U
#define VOLTAGE_MIN_V             (-0.1f)
#define VOLTAGE_MAX_V             10.5f
#define SAMPLE_READ_RETRY_MAX     3U
#define SAMPLE_UPDATED_CURRENT    0x01U
#define SAMPLE_UPDATED_VOLTAGE1   0x02U
#define SAMPLE_UPDATED_VOLTAGE2   0x04U
#define VOLTAGE1_CAL_K    1.00499f
#define VOLTAGE1_CAL_B   (-0.228f)
#define VOLTAGE_ZERO_CLAMP_V   0.01f

#define VOLTAGE2_CAL_K   0.99852f
#define VOLTAGE2_CAL_B  (-0.20490f)

#define CURRENT_CAL_K   1.0f
#define CURRENT_CAL_B   0
 
#ifndef SAMPLE_TEST_MODE
#define SAMPLE_TEST_MODE 0

#endif

#define AD3344_CFG_COMMON \
    (AD3344_REG_CONFIG_OS_SINGLE | AD3344_REG_CONFIG_MODE_SINGLE | \
     AD3344_REG_CONFIG_DR_100SPS | AD3344_REG_CONFIG_PULL_UP_EN | \
     AD3344_REG_CONFIG_NOP_VALID | AD3344_RESERVED_VALUE)
#define AD3344_CFG_CURRENT \
    (AD3344_CFG_COMMON | \
     AD3344_REG_CONFIG_MUX_DIFF_0_1 | \
     AD3344_REG_CONFIG_PGA_4_096V)
#define AD3344_CFG_VOLTAGE1 \
    (AD3344_CFG_COMMON | AD3344_REG_CONFIG_MUX_SINGLE_2 | \
     AD3344_REG_CONFIG_PGA_4_096V)
#define AD3344_CFG_VOLTAGE2 \
    (AD3344_CFG_COMMON | AD3344_REG_CONFIG_MUX_SINGLE_3 | \
     AD3344_REG_CONFIG_PGA_4_096V)

#define SAMPLE_RAW_DIFF_MAX  200

SampleData_t g_sample_data;
SampleDebugData_t g_sample_debug = {0};

static int16_t g_last_valid_current_raw;
static int16_t g_last_valid_voltage1_raw;
static int16_t g_last_valid_voltage2_raw;

// 将ADC差分电压转换为实际电流，考虑放大器增益和分流电阻
static float Sample_Calibrate(float value, float k, float b)
{
    return value * k + b;
}

// 将ADC差分电压转换为实际电流，考虑放大器增益和分流电阻
static float Sample_CurrentRaw2AdcVoltage(int16_t raw)
{
    return ((float)raw * CURRENT_ADC_FSR) / ADC_SIGNED_FULL_SCALE;
}

// 将ADC差分电压转换为实际电流，考虑放大器增益和分流电阻
static float Sample_VoltageRaw2AdcVoltage(int16_t raw)
{
    return ((float)raw * VOLTAGE_ADC_FSR) / ADC_SIGNED_FULL_SCALE;
}

// 将ADC差分电压转换为实际电流，考虑放大器增益和分流电阻
static float Sample_ConvertCurrent(float adc_diff_voltage)
{
    float current_A;

    current_A = adc_diff_voltage /
                (AMC_GAIN_NOMINAL * CURRENT_SHUNT_NOMINAL);
    return current_A * 1000.0f;
}

// 将ADC电压转换为实际电压，考虑分压器、放大器增益和差分转单端增益
static float Sample_ConvertVoltage(float adc_voltage)
{
    return (adc_voltage - VOLTAGE_OFFSET_NOMINAL) /
           (VOLTAGE_DIV_RATIO * AMC_GAIN_NOMINAL * DIFF_TO_SE_GAIN);
}

// 读取电流断路状态，若电流低于断路阈值，则置位断路标志；若电流高于恢复阈值，则清除断路标志
static void Sample_CheckCurrentBreak(void)
{
    static uint8_t break_count;
    static uint8_t recover_count;
    const SystemParam_t *param;

    param = App_ParamGet();


    if(g_sample_data.current_break == 0U)
    {
        if(g_sample_data.current_mA < param->current_recover_ma)
        {
            if(break_count < CURRENT_BREAK_CONFIRM_CNT)
            {
                break_count++;
            }
            if(break_count >= CURRENT_BREAK_CONFIRM_CNT)
            {
                g_sample_data.current_break = 1U;
                break_count = 0U;
                recover_count = 0U;
            }
        }
        else
        {
            break_count = 0U;
        }
    }
    else if(g_sample_data.current_mA > param->current_recover_ma)
    {
        if(recover_count < CURRENT_BREAK_CONFIRM_CNT)
        {
            recover_count++;
        }
        if(recover_count >= CURRENT_BREAK_CONFIRM_CNT)
        {
            g_sample_data.current_break = 0U;
            break_count = 0U;
            recover_count = 0U;
        }
    }
    else
    {
        recover_count = 0U;
    }

}

static void Sample_CheckVoltageError(uint8_t updated_mask)
{
    if((updated_mask & SAMPLE_UPDATED_VOLTAGE1) != 0U)
    {
        g_sample_data.voltage1_error =
            (g_sample_data.voltage1_V < VOLTAGE_MIN_V) ||
            (g_sample_data.voltage1_V > VOLTAGE_MAX_V);
    }

    if((updated_mask & SAMPLE_UPDATED_VOLTAGE2) != 0U)
    {
        g_sample_data.voltage2_error =
            (g_sample_data.voltage2_V < VOLTAGE_MIN_V) ||
            (g_sample_data.voltage2_V > VOLTAGE_MAX_V);
    }
}

static int Sample_ReadChannel(uint16_t config, int16_t *raw)
{
    if(raw == 0)
    {
        return -1;
    }

    return ad3344_read_data16(config, raw);
}

// 读取通道，确保两次读取结果接近，才认为有效
static int Sample_ReadChannelStable(
    uint16_t config,
    int16_t *result)
{
    int16_t dummy;
    int16_t value1;
    int16_t value2;
    int32_t diff;
    int ret;
    uint8_t attempt;

    if(result == 0)
    {
        return -1;
    }

    ret = -1;

    for(attempt = 0U;
        attempt < SAMPLE_READ_RETRY_MAX;
        attempt++)
    {
        /* 切换MUX后的第一笔丢弃 */
        ret = Sample_ReadChannel(config, &dummy);
        if(ret != 0)
        {
            continue;
        }

        /* 第一笔候选值 */
        ret = Sample_ReadChannel(config, &value1);
        if(ret != 0)
        {
            continue;
        }

        /* 第二笔候选值 */
        ret = Sample_ReadChannel(config, &value2);
        if(ret != 0)
        {
            continue;
        }

        diff = (int32_t)value1 - (int32_t)value2;

        if(diff < 0)
        {
            diff = -diff;
        }

        /*
         * 两次读取足够接近才认为可信
         */
        if(diff <= SAMPLE_RAW_DIFF_MAX)
        {
            *result =
                (int16_t)(((int32_t)value1 +
                           (int32_t)value2) / 2);

            return 0;
        }
    }

    return -2;
}

// 递增错误计数器，避免溢出
static void Sample_IncrementErrorCount(uint32_t *count)
{
    if((count != 0) && (*count < 0xFFFFFFFFUL))
    {
        (*count)++;
    }
}

// 读取电流、电压1、电压2的原始ADC值，并更新g_sample_data和g_sample_debug
static uint8_t Sample_ReadRaw(void)
{
    static uint8_t channel = 0U;
    int16_t value;
    uint8_t updated_mask = 0U;

    switch(channel)
    {
        case 0U:
            if(Sample_ReadChannelStable(AD3344_CFG_VOLTAGE1, &value) == 0)      //读取电压1通道
            {
                g_last_valid_voltage1_raw = value;
                g_sample_data.voltage1_raw = value;
                g_sample_debug.voltage1_valid = 1U;
                g_sample_debug.voltage1_read_error = 0U;
                updated_mask = SAMPLE_UPDATED_VOLTAGE1;    //置更新标志
            }
            else
            {
                g_sample_debug.voltage1_read_error = 1U;    //置错误标志
                Sample_IncrementErrorCount(&g_sample_debug.voltage1_read_error_count);
                if(g_sample_debug.voltage1_valid != 0U)
                {
                    g_sample_data.voltage1_raw = g_last_valid_voltage1_raw;
                }
            }
            channel = 1U;
            break;

        case 1U:
            if(Sample_ReadChannelStable(AD3344_CFG_VOLTAGE2, &value) == 0)
            {
                g_last_valid_voltage2_raw = value;
                g_sample_data.voltage2_raw = value;
                g_sample_debug.voltage2_valid = 1U;
                g_sample_debug.voltage2_read_error = 0U;
                updated_mask = SAMPLE_UPDATED_VOLTAGE2;     //置更新标志
            }
            else
            {
                g_sample_debug.voltage2_read_error = 1U;
                Sample_IncrementErrorCount(&g_sample_debug.voltage2_read_error_count);
                if(g_sample_debug.voltage2_valid != 0U)
                {
                    g_sample_data.voltage2_raw = g_last_valid_voltage2_raw;
                }
            }

            channel = 2U;
            break;

        case 2U:
                        if(Sample_ReadChannelStable(AD3344_CFG_CURRENT, &value) == 0)
            {
                g_last_valid_current_raw = value;
                g_sample_data.current_raw = value;
                g_sample_debug.current_valid = 1U;
                g_sample_debug.current_read_error = 0U;
                updated_mask = SAMPLE_UPDATED_CURRENT;
            }
            else
            {
                g_sample_debug.current_read_error = 1U;
                Sample_IncrementErrorCount(&g_sample_debug.current_read_error_count);
                if(g_sample_debug.current_valid != 0U)
                {
                    g_sample_data.current_raw = g_last_valid_current_raw;
                }
            }
            channel = 0U;
            break;

        default:
            channel = 0U;
            break;
    }

    return updated_mask;
}

void Sample_Init(void)
{
    memset(&g_sample_data, 0, sizeof(g_sample_data));
    memset(&g_sample_debug, 0, sizeof(g_sample_debug));
    g_last_valid_current_raw = 0;
    g_last_valid_voltage1_raw = 0;
    g_last_valid_voltage2_raw = 0;

    AD3344_CONFIG = AD3344_CONFIG_DEFAULT;

#if SAMPLE_TEST_MODE == 0
    ad3344_spi_init();      // 初始化SPI接口

    ad3344_process();
    // ad3344_ExtRef();

    AD3344_CONFIG = AD3344_CONFIG_DEFAULT;
    ad3344_init(AD3344_CONFIG);     // 初始化AD3344芯片
#endif
}

float Sample_Getch0(void)
{
    Sample_ReadRaw(); // 读取最新的原始值，确保返回的值是最新的
    return (float)g_sample_data.voltage1_raw * g_system_params.ch0_ratio;
}

float Sample_Getch1(void)
{
    Sample_ReadRaw(); // 读取最新的原始值，确保返回的值是最新的
    return (float)g_sample_data.voltage2_raw * g_system_params.ch1_ratio;

}

float Sample_Getch2(void)
{
    Sample_ReadRaw(); // 读取最新的原始值，确保返回的值是最新的
    return (g_sample_data.current_raw - CURRENT_4MA_RAW) * g_system_params.ch2_ratio;
}


// 采样任务，读取ADC原始值，计算实际电流和电压，并更新g_sample_data和g_sample_debug
void Sample_Task(void)
{
    
    float current_adc_voltage;
    float voltage1_adc_voltage;
    float voltage2_adc_voltage;

    uint8_t updated_mask;

    //读取adc原始值
    updated_mask = Sample_ReadRaw();

    if((updated_mask & SAMPLE_UPDATED_CURRENT) != 0U)
    {
        current_adc_voltage =
            Sample_CurrentRaw2AdcVoltage(g_sample_data.current_raw);

        /*
        g_sample_debug.current_adc_mv =
            (int32_t)(current_adc_voltage * 1000.0f +
                      ((current_adc_voltage >= 0.0f) ? 0.5f : -0.5f));

        g_sample_data.current_mA = Sample_Calibrate(
            Sample_ConvertCurrent(current_adc_voltage), g_app_param.current_k, g_app_param.current_b);
        */

        g_sample_data.current_mA = (g_sample_data.current_raw - CURRENT_4MA_RAW) * g_system_params.ch2_ratio;

        Sample_CheckCurrentBreak();
    }

    if((updated_mask & SAMPLE_UPDATED_VOLTAGE1) != 0U)
    {
        voltage1_adc_voltage =
            Sample_VoltageRaw2AdcVoltage(g_sample_data.voltage1_raw);

        /*
        g_sample_debug.voltage1_adc_mv =
            (int32_t)(voltage1_adc_voltage * 1000.0f +
                      ((voltage1_adc_voltage >= 0.0f) ? 0.5f : -0.5f));


        g_sample_data.voltage1_V = Sample_Calibrate(
            Sample_ConvertVoltage(voltage1_adc_voltage), g_app_param.voltage1_k, g_app_param.voltage1_b);
                      */
                     
        g_sample_data.voltage1_V = g_sample_data.voltage1_raw * g_system_params.ch0_ratio;


        if((g_sample_data.voltage1_V > -VOLTAGE_ZERO_CLAMP_V) &&
        (g_sample_data.voltage1_V < VOLTAGE_ZERO_CLAMP_V))
        {
            g_sample_data.voltage1_V = 0.0f;
        }

    }

    if((updated_mask & SAMPLE_UPDATED_VOLTAGE2) != 0U)
    {
        voltage2_adc_voltage =
            Sample_VoltageRaw2AdcVoltage(g_sample_data.voltage2_raw);

            /*
            g_sample_debug.voltage2_adc_mv =
            (int32_t)(voltage2_adc_voltage * 1000.0f +
                      ((voltage2_adc_voltage >= 0.0f) ? 0.5f : -0.5f));            


        g_sample_data.voltage2_V = Sample_Calibrate(
            Sample_ConvertVoltage(voltage2_adc_voltage), g_app_param.voltage2_k, g_app_param.voltage2_b);
                        */
        g_sample_data.voltage2_V = g_sample_data.voltage2_raw * g_system_params.ch1_ratio;

        if((g_sample_data.voltage2_V > -VOLTAGE_ZERO_CLAMP_V) &&
        (g_sample_data.voltage2_V < VOLTAGE_ZERO_CLAMP_V))
        {
            g_sample_data.voltage2_V = 0.0f;
        }

    }

    Sample_CheckVoltageError(updated_mask);

    //通信失败/从未有过有效值时，通道对外明确报异常。 
    g_sample_data.current_error =
        (g_sample_data.current_break != 0U) ||
        (g_sample_debug.current_read_error != 0U) ||
        (g_sample_debug.current_valid == 0U);

    if((g_sample_debug.voltage1_read_error != 0U) ||
       (g_sample_debug.voltage1_valid == 0U))
    {
        g_sample_data.voltage1_error = 1U;
    }

    if((g_sample_debug.voltage2_read_error != 0U) ||
       (g_sample_debug.voltage2_valid == 0U))
    {
        g_sample_data.voltage2_error = 1U;
    }

    //采样计数
    g_sample_data.sample_count++;
    
}

void Sample_SetCurrentError(uint8_t error)
{
    g_sample_data.current_error = (error != 0U) ? 1U : 0U;
}

uint8_t Sample_GetCurrentError(void)
{
    return g_sample_data.current_error;
}


static int32_t Sample_FloatToX100(float value)
{
    if(value >= 0.0f)
    {
        return (int32_t)(value * 100.0f + 0.5f);
    }
    else
    {
        return (int32_t)(value * 100.0f - 0.5f);
    }
}



void Sample_DebugPrint(void)
{
    
    static char tx_buf[240];

    int32_t current_x100;
    int32_t voltage1_x100;
    int32_t voltage2_x100;

    uint32_t current_abs;
    uint32_t voltage1_abs;
    uint32_t voltage2_abs;

    const char *current_sign;
    const char *voltage1_sign;
    const char *voltage2_sign;

    int len;

    current_x100 =
        Sample_FloatToX100(g_sample_data.current_mA);

    voltage1_x100 =
        Sample_FloatToX100(g_sample_data.voltage1_V);

    voltage2_x100 =
        Sample_FloatToX100(g_sample_data.voltage2_V);


    current_sign =
        (current_x100 < 0) ? "-" : "";

    voltage1_sign =
        (voltage1_x100 < 0) ? "-" : "";

    voltage2_sign =
        (voltage2_x100 < 0) ? "-" : "";


    current_abs =
        (current_x100 < 0) ?
        (uint32_t)(-current_x100) :
        (uint32_t)current_x100;

    voltage1_abs =
        (voltage1_x100 < 0) ?
        (uint32_t)(-voltage1_x100) :
        (uint32_t)voltage1_x100;

    voltage2_abs =
        (voltage2_x100 < 0) ?
        (uint32_t)(-voltage2_x100) :
        (uint32_t)voltage2_x100;


    len = snprintf(
        tx_buf,
        sizeof(tx_buf),

        "I:%d %ldmV %s%lu.%02lumA | "
        "V1:%d %ldmV %s%lu.%02luV | "
        "V2:%d %ldmV %s%lu.%02luV | "
        "ERR:I=%u/%lu V1=%u/%lu V2=%u/%lu\r\n",

        (int)g_sample_data.current_raw,
        (long)g_sample_debug.current_adc_mv,
        current_sign,
        (unsigned long)(current_abs / 100U),
        (unsigned long)(current_abs % 100U),

        (int)g_sample_data.voltage1_raw,
        (long)g_sample_debug.voltage1_adc_mv,
        voltage1_sign,
        (unsigned long)(voltage1_abs / 100U),
        (unsigned long)(voltage1_abs % 100U),

        (int)g_sample_data.voltage2_raw,
        (long)g_sample_debug.voltage2_adc_mv,
        voltage2_sign,
        (unsigned long)(voltage2_abs / 100U),
        (unsigned long)(voltage2_abs % 100U),

        (unsigned int)g_sample_debug.current_read_error,
        (unsigned long)
        g_sample_debug.current_read_error_count,

        (unsigned int)g_sample_debug.voltage1_read_error,
        (unsigned long)
        g_sample_debug.voltage1_read_error_count,

        (unsigned int)g_sample_debug.voltage2_read_error,
        (unsigned long)
        g_sample_debug.voltage2_read_error_count
    );

    if(len <= 0)
    {
        return;
    }

    if(len >= (int)sizeof(tx_buf))
    {
        len = sizeof(tx_buf) - 1;
    }

    USART485_Send(
        (uint8_t *)tx_buf,
        (uint16_t)len);
        

   
}
