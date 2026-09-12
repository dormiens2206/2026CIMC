#include "DAC.h"

void DAC_Init(void)
{
    //开启时钟
    rcu_periph_clock_enable(DAC_GPIO_RCU);
    rcu_periph_clock_enable(DACX_RCU);

    //PA4、PA5配置为模拟模式
    gpio_mode_set(DAC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC0_PIN | DAC1_PIN);

    //复位DAC外设
    dac_deinit(DACX_PERIPH);

    //配置通道0
    dac_trigger_disable(DACX_PERIPH, DAC_OUT0);
    dac_wave_mode_config(DACX_PERIPH, DAC_OUT0, DAC_WAVE_DISABLE);
    dac_output_buffer_enable(DACX_PERIPH, DAC_OUT0);

    dac_data_set(DACX_PERIPH, DAC_OUT0, DAC_ALIGN_12B_R, 0); // 初始输出 0V
    dac_enable(DACX_PERIPH, DAC_OUT0);

    //配置通道1
    dac_trigger_disable(DACX_PERIPH, DAC_OUT1);
    dac_wave_mode_config(DACX_PERIPH, DAC_OUT1, DAC_WAVE_DISABLE);
    dac_output_buffer_enable(DACX_PERIPH, DAC_OUT1);

    dac_data_set(DACX_PERIPH, DAC_OUT1, DAC_ALIGN_12B_R, 0); // 初始输出 0V
    dac_enable(DACX_PERIPH, DAC_OUT1);
}

void DAC_Set_Raw(uint8_t channel, uint16_t raw)
{
    //防止溢出乱码
    if (raw > DAC_MAX_RAW) 
    {
        raw = DAC_MAX_RAW;
    }

    //设置输出
    dac_data_set(DACX_PERIPH, channel, DAC_ALIGN_12B_R, raw);
}

void DAC_Set_Voltage(uint8_t channel, float voltage)
{
    uint16_t raw;

    //边界保护与电压换算
    if (voltage <= 0.0f) 
    {
        raw = 0;
    } 
    else if (voltage >= DAC_VREF) 
    {
        raw = DAC_MAX_RAW;
    } 
    else 
    {

        raw = (uint16_t)((voltage / DAC_VREF) * DAC_MAX_RAW + 0.5f);
    }

    DAC_Set_Raw(channel, raw);
}
