#ifndef __DAC_H
#define __DAC_H

#include "HeaderFiles.h"

#define DAC_VREF        3.3f
#define DAC_MAX_RAW     4095U


#define DACX_PERIPH     DAC0
#define DACX_RCU        RCU_DAC

#define DAC_GPIO_RCU    RCU_GPIOA 
#define DAC_PORT        GPIOA
#define DAC0_PIN        GPIO_PIN_4
#define DAC1_PIN        GPIO_PIN_5

#define DAC0_CHANNEL    DAC_OUT0
#define DAC1_CHANNEL    DAC_OUT1

void DAC_Init(void);

void DAC_Set_Voltage(uint8_t channel, float voltage);
void DAC_Set_Raw(uint8_t channel, uint16_t raw);

#endif
