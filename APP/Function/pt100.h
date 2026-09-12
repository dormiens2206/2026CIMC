#ifndef __PT_100_H
#define __PT_100_H

#include "HeaderFiles.h"

typedef struct
{
    float measured;   // 实测平均温度
    float standard;   // 理论温度
} PT100_CalibPoint;


extern const float temp_C[12];
extern const float res_R[12];

extern float g_pt100_voltage;
extern float g_pt100_real_voltage;
extern float g_pt100_res;
extern float g_pt100_temp_raw;

float PT100_Calculate_Temperature(float current_res);
void PT100_Read(float *temperature);
float PT100_GetTemperature(void);
float PT100_Filter(float new_temp);
float PT100_Compensate(float temp);
void AD3344_reg_Config(uint8_t InputMUX, uint8_t Channel);
float adcdata_to_volt(uint16_t bin);
void PT100_Init(void);

#endif
