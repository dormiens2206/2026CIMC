#include "pt100.h"

const float temp_C[12] = {-49.27f, -44.49f, 0.0f, 20.0f, 33.44f, 38.61f, 40.0f, 60.0f, 80.0f, 100.0f, 130.45f, 141.11f};
const float res_R[12] = {80.6f, 82.5f, 100.0f, 107.79f, 113.0f, 115.0f, 115.54f, 123.24f, 130.90f, 138.51f, 150.0f, 154.0f};

float g_pt100_voltage = 0.0f;
float g_pt100_real_voltage = 0.0f;
float g_pt100_res = 0.0f;
float g_pt100_temp_raw = 0.0f;

const PT100_CalibPoint pt100_calib_table[] =
{
    {-46.58f, -49.27f},
    {-41.56f, -44.49f},
    {3.38f,    0.00f},
    {23.73f,   20.00f},
    {35.54f,   33.44f},
    {42.93f,   38.61f},
    {44.35f,   40.00f},
    {63.59f,   60.00f},
    {77.82f,   80.00f},
    {81.57f,   100.00f},
    {87.07f,   130.45f},
    {88.99f,   141.11f},
};
//转换电压必备的宏
#define POSITIVE_FS 0x7FFF
#define NEGATIVE_FS 0x8000
#define ADC_DATA    32768

static float pga = 5.0f; // 满量程换算系数
extern uint16_t AD3344_CONFIG; // 引用官方驱动库里的全局配置变量

//寄存器配置函数
void AD3344_reg_Config(uint8_t InputMUX, uint8_t Channel)
{
    AD3344_CONFIG = AD3344_CONFIG_DEFAULT;

    if(InputMUX == AD3344_DUAL_END)
    {
        switch (Channel)
        {
        case (0): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_1; break;
        case (1): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_3; break;
        case (2): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_1_3; break;
        case (3): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_2_3; break;
        }
    }else if(InputMUX == AD3344_SINGLE_END)
    {
        switch (Channel)
        {
        case (0): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_0; break;
        case (1): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_1; break;
        case (2): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_2; break;
        case (3): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_3; break;
        }
    }
    
    AD3344_CONFIG &= ~AD3344_REG_CONFIG_DR_MASK;
    AD3344_CONFIG |= AD3344_REG_CONFIG_DR_1000SPS;

    AD3344_CONFIG &= ~AD3344_REG_CONFIG_PGA_MASK;
    AD3344_CONFIG |= AD3344_REG_CONFIG_PGA_4_096V;

    // 量程配置：实际数值需结合 GD30AD3344 手册与外部基准定义确认
 
    
    AD3344_CONFIG |= AD3344_REG_CONFIG_PULL_UP_EN;
    AD3344_CONFIG |= AD3344_REG_CONFIG_NOP_VALID;
    AD3344_CONFIG |= AD3344_REG_CONFIG_MODE_SINGLE;
    
}

//二进制转电压函数
float adcdata_to_volt(uint16_t bin)
{
    int  _val;
    float adcValue;
    
    if(bin == NEGATIVE_FS){
        adcValue = -(pga*bin/ADC_DATA);
    }else{
        _val     =  bin&NEGATIVE_FS ?  (-((~bin+1)&POSITIVE_FS)):bin;
        adcValue = pga*_val/ADC_DATA;
    }
    return adcValue;
}

void PT100_Init(void)
{
    ad3344_spi_init();
    ad3344_process();
    ad3344_ExtRef();

    AD3344_reg_Config(AD3344_SINGLE_END, 0);
    ad3344_init(AD3344_CONFIG);
}


float PT100_Calculate_Temperature(float current_res)
{
    if(current_res <= res_R[0]) {
        return temp_C[0];
    }
    if(current_res >= res_R[11]) {
        return temp_C[11];
    }

    for(uint8_t i = 0; i < 11; i++) {
        if(current_res >= res_R[i] && current_res <= res_R[i + 1]) {
            return temp_C[i] + (current_res - res_R[i]) * (temp_C[i + 1] - temp_C[i]) / (res_R[i + 1] - res_R[i]);
        }
    }

    return temp_C[0];
}

void PT100_Read(float *temperature)
{
    if(temperature == NULL) 
    {
        return; 
    }
    /* 启动单次转换并读取结果（ad3344_read_data16 内部完成写配置+等待+读数） */
    AD3344_CONFIG = (AD3344_CONFIG & ~AD3344_REG_CONFIG_OS_MASK) | AD3344_REG_CONFIG_OS_SINGLE;
    int16_t raw_signed = 0;
    uint16_t raw_data;

    if(ad3344_read_data16(AD3344_CONFIG, &raw_signed) != 0)
    {
        return;
    }

    raw_data = (uint16_t)raw_signed;

    //将原始数据转换为电压值，这里算出来的电压是ADC引脚测量到的放大后的电压
    float voltage = adcdata_to_volt(raw_data);   
		
    /*
    char debug_str[60];
    //串口打印原始数据和电压值，方便调试
    sprintf(debug_str, "Debug -> RAW: 0x%04X, Volt: %.3f V\r\n", raw_data, voltage);
    USART485_Send((uint8_t *)debug_str, strlen(debug_str));
    */

    if (voltage < 0.01f || voltage > 2.45f) 
    {
        *temperature = -999.0f; 
        return;
    }

    //还原为真实的微小电压
    float real_pt100_voltage = voltage / 10.0f; 
    //根据电压值计算PT100的电阻值
    //1mA的电流下，电压=电流*电阻，所以电阻=电压/电流=电压/0.001A=电压*1000
    float current_res = real_pt100_voltage * 1000.0f; 
    float temp_raw = PT100_Calculate_Temperature(current_res);
    *temperature = PT100_Compensate(temp_raw);

    g_pt100_voltage = voltage;
    g_pt100_real_voltage = real_pt100_voltage;
    g_pt100_res = current_res;
    g_pt100_temp_raw = temp_raw;

    //根据电阻值计算温度
    //*temperature = PT100_Calculate_Temperature(current_res);

}

float PT100_GetTemperature(void)
{
    float temp = -999.0f;
    PT100_Read(&temp);
    return temp;
}


float PT100_Compensate(float temp)
{
    int n = sizeof(pt100_calib_table) / sizeof(pt100_calib_table[0]);

    if(temp <= pt100_calib_table[0].measured)
        return pt100_calib_table[0].standard;

    if(temp >= pt100_calib_table[n - 1].measured)
        return pt100_calib_table[n - 1].standard;

    for(int i = 0; i < n - 1; i++)
    {
        float x1 = pt100_calib_table[i].measured;
        float y1 = pt100_calib_table[i].standard;
        float x2 = pt100_calib_table[i + 1].measured;
        float y2 = pt100_calib_table[i + 1].standard;

        if(temp >= x1 && temp <= x2)
        {
            return y1 + (temp - x1) * (y2 - y1) / (x2 - x1);
        }
    }

    return temp;
}

/*
float PT100_Filter(float new_temp)
{
    static float filtered = 0.0f;
    static uint8_t first = 1;

    if(first)
    {
        filtered = new_temp;
        first = 0;
        return filtered;
    }

    // 如果温度突然变化超过 10℃，说明你切换了跳线帽/挡位
    // 这时不要慢慢滤波，直接更新
    if((new_temp - filtered > 10.0f) || (filtered - new_temp > 10.0f))
    {
        filtered = new_temp;
        return filtered;
    }

    // 小范围波动才进行平滑滤波
    filtered = filtered * 0.7f + new_temp * 0.3f;

    return filtered;
}
*/
