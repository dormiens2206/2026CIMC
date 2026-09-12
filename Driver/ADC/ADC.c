#include "ADC.h"

void ADC_Init(void)
{
	//使能ADC时钟
    rcu_periph_clock_enable(ADC_RCU);
    
    rcu_periph_clock_enable(ADC_CH0_RCU);
    rcu_periph_clock_enable(ADC_CH1_RCU);

    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);     //配置ADC时钟，PCLK2的8分频，得到6MHz的ADC时钟
    
    //配置ADC0通道引脚,设置为模拟输入模式
    gpio_mode_set(ADC_CH0_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_CH0_PIN);

    gpio_mode_set(ADC_CH1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_CH1_PIN);
    
    //复位ADC
    adc_deinit();
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);                     //独立模式
    adc_special_function_config(ADCx, ADC_SCAN_MODE, DISABLE);           //单通道模式
    adc_special_function_config(ADCx, ADC_CONTINUOUS_MODE, DISABLE);     //单次转换模式
    adc_data_alignment_config(ADCx, ADC_DATAALIGN_RIGHT);                //右对齐
    adc_channel_length_config(ADCx, ADC_ROUTINE_CHANNEL, 1);
    adc_external_trigger_config(ADCx, ADC_ROUTINE_CHANNEL, DISABLE);


    adc_enable(ADCx); //使能ADC
    delay_1ms(10);    //等待ADC稳定
    adc_calibration_enable(ADCx);   //校准ADC

}

//读取ADC原始值
uint16_t ADC_Read_Raw(uint8_t channel)
{
    adc_routine_channel_config(ADCx, 0, channel, ADC_SAMPLETIME_480);
    adc_flag_clear(ADCx, ADC_FLAG_EOC);      //清除残留标志

    adc_software_trigger_enable(ADCx, ADC_ROUTINE_CHANNEL);

    uint32_t timeout = 1000000; //超时计数，防止死循环
    while (!adc_flag_get(ADCx, ADC_FLAG_EOC)) 
    {
        timeout--;
        if (timeout == 0) 
        {
            return 0; //超时返回0，表示读取失败
        }
    }
    adc_flag_clear(ADCx, ADC_FLAG_EOC);     //清除结束标志

    return adc_routine_data_read(ADCx);
}

//将ADC原始值转换为电压值
float ADC_Raw_To_Voltage(uint16_t raw)
{
    float voltage = (raw / ADC_RESOLUTION) * ADC_REF_VOLTAGE; //假设参考电压为3.3V，12位分辨率
    return voltage;
}

//读取ADC电压值
float ADC_Read_Voltage(uint8_t channel)
{
    uint16_t raw_value = ADC_Read_Raw(channel);
    return ADC_Raw_To_Voltage(raw_value);
}

//求平均值滤波
uint16_t ADC_Read_Average(uint8_t channel, uint8_t sample)
{
    uint32_t sum = 0;

    if(sample == 0)
    {
        return 0; //避免除以0
    }

    for (uint8_t i = 0; i < sample; i++) 
    {
        sum += ADC_Read_Raw(channel);
    }
    uint16_t average = sum / sample;
    return average;
}

//电压报警
uint8_t ADC_Check_Alarm(uint8_t channel, float threshold)
{
    float voltage = ADC_Read_Voltage(channel);
    if (voltage > threshold) 
    {
        /*
        报警处理代码，例如点亮LED、发送通知等
        */
       
        return 1; //超过阈值返回1
    }
    return 0; //超过阈值返回1，否则返回0
}


