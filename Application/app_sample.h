#ifndef APP_SAMPLE_H
#define APP_SAMPLE_H

#include <stdint.h>

typedef struct
{
    int16_t current_raw;
    int16_t voltage1_raw;
    int16_t voltage2_raw;
    float current_mA;
    float voltage1_V;
    float voltage2_V;
    uint8_t current_break;
    uint8_t current_error;
    uint8_t voltage1_error;
    uint8_t voltage2_error;
    uint32_t sample_count;
} SampleData_t;

typedef struct
{
    int32_t current_adc_mv;
    int32_t voltage1_adc_mv;
    int32_t voltage2_adc_mv;
    uint32_t current_read_error_count;
    uint32_t voltage1_read_error_count;
    uint32_t voltage2_read_error_count;
    uint8_t current_valid;
    uint8_t voltage1_valid;
    uint8_t voltage2_valid;
    uint8_t current_read_error;
    uint8_t voltage1_read_error;
    uint8_t voltage2_read_error;
} SampleDebugData_t;

extern SampleData_t g_sample_data;
extern SampleDebugData_t g_sample_debug;

void Sample_Init(void);
void Sample_Task(void);
void Sample_SetCurrentError(uint8_t error);
uint8_t Sample_GetCurrentError(void);
void Sample_DebugPrint(void);

float Sample_Getch0(void);
float Sample_Getch1(void);
float Sample_Getch2(void);



#endif
