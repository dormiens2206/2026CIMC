#include "sample_task.h"

/*
 * 三个数据通道：
 * CH0：电位器 ADC 原始值 * CH0 变比
 * CH1：DAC 回读 ADC 原始值 * CH1 变比
 * CH2：PT100 实际温度值
 */

#define SAMPLE_AVG_TIMES    8

static float Sample_ReadAdcRawAverage(uint8_t adc_channel)
{
	uint16_t raw;

	raw = ADC_Read_Average(adc_channel, SAMPLE_AVG_TIMES);

	return (float)raw;
}

float Sample_GetCH0(void)
{
	float raw;
	float value;

	raw = Sample_ReadAdcRawAverage(ADC_CH0_CHANNEL);
	value = raw * g_system_params.ch0_ratio;

	return value;
}

float Sample_GetCH1(void)
{
	float raw;
	float value;

	raw = Sample_ReadAdcRawAverage(ADC_CH1_CHANNEL);
	value = raw * g_system_params.ch1_ratio;

	return value;
}

float Sample_GetPT100(void)
{
	float temp;

	temp = PT100_GetTemperature();

	return temp;
}

void Sample_SetCH0Ratio(float ratio)
{
    Param_SetCH0Ratio(ratio);
}

void Sample_SetCH1Ratio(float ratio)
{
    Param_SetCH1Ratio(ratio);
}
