#include "app_param.h"
#include "flash_param.h"

#include <string.h>

#define APP_DEFAULT_ADDRESS         1U
#define APP_DEFAULT_BAUD_CODE       0x13U
#define APP_DEFAULT_SAMPLE_MS       1000U
#define APP_DEFAULT_STORAGE_MS      1000U
#define APP_DEFAULT_STORAGE_ENABLE  1U
#define APP_DEFAULT_CURRENT_BREAK_MA    3.60f
#define APP_DEFAULT_CURRENT_RECOVER_MA  3.80f

SystemParam_t g_app_param;


/*
 * 波特率编码转实际波特率
 */
uint32_t App_ParamBaudCodeToRate(uint8_t baud_code)
{
    switch(baud_code)
    {
        case 0x11U:
            return 4800U;

        case 0x12U:
            return 9600U;

        case 0x13U:
            return 19200U;

        case 0x14U:
            return 115200U;

        default:
            return 0U;
    }
}


/*
 * APP 参数恢复默认
 */
void App_ParamDefault(void)
{
    memset(&g_app_param, 0, sizeof(g_app_param));

    g_app_param.modbus_address =
        APP_DEFAULT_ADDRESS;

    g_app_param.baud_code =
        APP_DEFAULT_BAUD_CODE;

    g_app_param.sample_period_ms =
        APP_DEFAULT_SAMPLE_MS;

    g_app_param.storage_period_ms =
        APP_DEFAULT_STORAGE_MS;

    g_app_param.storage_enable =
        APP_DEFAULT_STORAGE_ENABLE;


    g_app_param.current_k = 1.0f;
    g_app_param.current_b = 0.0f;

    g_app_param.voltage1_k = 1.0f;
    g_app_param.voltage1_b = 0.0f;

    g_app_param.voltage2_k = 1.0f;
    g_app_param.voltage2_b = 0.0f;

    g_app_param.current_break_ma =
    APP_DEFAULT_CURRENT_BREAK_MA;

    g_app_param.current_recover_ma =
    APP_DEFAULT_CURRENT_RECOVER_MA;
}


/*
 * 检查 APP 参数是否合法
 */
uint8_t App_ParamValidate(const SystemParam_t *param)
{
    if(param == 0)
    {
        return 0U;
    }

    if(param->modbus_address == 0U ||
       param->modbus_address > 247U)
    {
        return 0U;
    }

    if(App_ParamBaudCodeToRate(param->baud_code) == 0U)
    {
        return 0U;
    }

    if(param->sample_period_ms == 0U)
    {
        return 0U;
    }

    if(param->storage_period_ms == 0U)
    {
        return 0U;
    }

    if(param->storage_enable > 1U)
    {
        return 0U;
    }

		
		/*
		 * 三个校准增益 K 必须大于 0。
		 *
		 * B 可以为正、负或 0，
		 * 因此这里不限制 B。
		 */
		if(param->current_k <= 0.0f)
		{
				return 0U;
		}

		if(param->voltage1_k <= 0.0f)
		{
				return 0U;
		}

		if(param->voltage2_k <= 0.0f)
		{
				return 0U;
		}
		
    return 1U;
}


/*
 * 从原有 g_system_params 中恢复
 * 决赛 APP 参数。
 *
 * 注意：
 * 不是 memcpy。
 * 两个结构体不同，只逐字段对应。
 */
static void App_ParamLoadFromSystem(void)
{
    g_app_param.modbus_address =
        g_system_params.modbus_address;

    g_app_param.baud_code =
        g_system_params.baud_code;

    g_app_param.sample_period_ms =
        g_system_params.sample_period_ms;

    g_app_param.storage_period_ms =
        g_system_params.storage_period_ms;

    g_app_param.storage_enable =
        g_system_params.storage_enable;


    g_app_param.current_k =
        g_system_params.current_k;

    g_app_param.current_b =
        g_system_params.current_b;

    g_app_param.voltage1_k =
        g_system_params.voltage1_k;

    g_app_param.voltage1_b =
        g_system_params.voltage1_b;

    g_app_param.voltage2_k =
        g_system_params.voltage2_k;

    g_app_param.voltage2_b =
        g_system_params.voltage2_b;

    g_app_param.current_break_ma =
        g_system_params.current_break_ma;

    g_app_param.current_recover_ma =
        g_system_params.current_recover_ma;

    
}


/*
 * 把 APP 参数同步回原有系统参数结构。
 *
 * 真正写 Flash 仍然统一使用 Param_Save()。
 */
static void App_ParamCopyToSystem(void)
{
    g_system_params.modbus_address =
        g_app_param.modbus_address;

    g_system_params.baud_code =
        g_app_param.baud_code;

    g_system_params.sample_period_ms =
        g_app_param.sample_period_ms;

    g_system_params.storage_period_ms =
        g_app_param.storage_period_ms;

    g_system_params.storage_enable =
        g_app_param.storage_enable;


    g_system_params.current_k =
        g_app_param.current_k;

    g_system_params.current_b =
        g_app_param.current_b;

    g_system_params.voltage1_k =
        g_app_param.voltage1_k;

    g_system_params.voltage1_b =
        g_app_param.voltage1_b;

    g_system_params.voltage2_k =
        g_app_param.voltage2_k;

    g_system_params.voltage2_b =
        g_app_param.voltage2_b;

    g_system_params.current_break_ma =
        g_app_param.current_break_ma;

    g_system_params.current_recover_ma =
        g_app_param.current_recover_ma;
}


/*
 * 初始化 APP 参数。
 *
 * System_Init() 中已经先执行：
 *
 * Param_Load();
 *
 * 因此这里直接从 g_system_params 获取即可。
 */
void App_ParamInit(void)
{
    App_ParamLoadFromSystem();


    /*
     * 正常情况下 Param_Load() 已经做过校验。
     * 这里再检查一次，作为第二道保险。
     */
    if(!App_ParamValidate(&g_app_param))
    {
        App_ParamDefault();

        App_ParamCopyToSystem();

        Param_Save();
    }
}


const SystemParam_t *App_ParamGet(void)
{
    return &g_app_param;
}


/*
 * 修改采样周期。
 *
 * 重点：
 * 如果数值没变化，不写 Flash。
 *
 * 因为这个函数会在主循环中反复被调用。
 */
uint8_t App_ParamSetSamplePeriod(uint16_t period_ms)
{
    if(period_ms == 0U)
    {
        return 0U;
    }


    if(g_app_param.sample_period_ms == period_ms)
    {
        return 1U;
    }


    g_app_param.sample_period_ms = period_ms;

    g_system_params.sample_period_ms = period_ms;

    Param_Save();


    return 1U;
}


/*
 * 修改 TF 卡存储周期。
 */
uint8_t App_ParamSetStoragePeriod(uint16_t period_ms)
{
    if(period_ms == 0U)
    {
        return 0U;
    }


    if(g_app_param.storage_period_ms == period_ms)
    {
        return 1U;
    }


    g_app_param.storage_period_ms = period_ms;

    g_system_params.storage_period_ms = period_ms;

    Param_Save();


    return 1U;
}


/*
 * 修改 TF 卡存储开关。
 */
uint8_t App_ParamSetStorageEnable(uint8_t enable)
{
    if(enable > 1U)
    {
        return 0U;
    }


    if(g_app_param.storage_enable == enable)
    {
        return 1U;
    }


    g_app_param.storage_enable = enable;

    g_system_params.storage_enable = enable;

    Param_Save();


    return 1U;
}
