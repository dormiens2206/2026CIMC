#ifndef APP_PARAM_H
#define APP_PARAM_H

#include <stdint.h>

typedef struct
{
    uint8_t modbus_address;
    uint8_t baud_code;
    uint16_t sample_period_ms;
    uint16_t storage_period_ms;
    uint8_t storage_enable;

    float current_k;
    float current_b;

    float voltage1_k;
    float voltage1_b;

    float voltage2_k;
    float voltage2_b;

    float current_break_ma;     // 断路电流阈值，单位 mA
    float current_recover_ma;       // 断路恢复电流阈值，单位 mA
} SystemParam_t;

extern SystemParam_t g_app_param;

void App_ParamInit(void);
void App_ParamDefault(void);
uint8_t App_ParamValidate(const SystemParam_t *param);
const SystemParam_t *App_ParamGet(void);
uint8_t App_ParamSetSamplePeriod(uint16_t period_ms);
uint8_t App_ParamSetStoragePeriod(uint16_t period_ms);
uint8_t App_ParamSetStorageEnable(uint8_t enable);
uint32_t App_ParamBaudCodeToRate(uint8_t baud_code);

#endif
