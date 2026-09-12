#ifndef __ADC_H
#define __ADC_H

#include "HeaderFiles.h"

#define ADCx               ADC0
#define ADC_RCU            RCU_ADC0
#define ADC_IRQn           ADC_IRQn

#define ADC_CH0_RCU         RCU_GPIOC
#define ADC_CH0_PORT        GPIOC
#define ADC_CH0_PIN         GPIO_PIN_0
#define ADC_CH0_CHANNEL     ADC_CHANNEL_10

//连接滑动变阻器的通道
#define ADC_CH1_RCU         RCU_GPIOC
#define ADC_CH1_PORT        GPIOC
#define ADC_CH1_PIN         GPIO_PIN_1
#define ADC_CH1_CHANNEL     ADC_CHANNEL_11

#define ADC_REF_VOLTAGE  3.2979f  // 根据 TL431 硬件测得的高精度基准
#define ADC_RESOLUTION   4095.0f

/*
#define ADC_CH1_RCU         RCU_GPIOC
#define ADC_CH1_PORT        GPIOC
#define ADC_CH1_PIN         GPIO_PIN_2
#define ADC_CH1_CHANNEL     ADC_CHANNEL_12
*/

void ADC_Init(void);
uint16_t ADC_Read_Raw(uint8_t channel);
float ADC_Read_Voltage(uint8_t channel);
float ADC_Raw_To_Voltage(uint16_t raw);
uint16_t ADC_Read_Average(uint8_t channel, uint8_t sample);
uint8_t ADC_Check_Alarm(uint8_t channel, float threshold);

#endif
