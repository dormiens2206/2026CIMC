#include "alarm_task.h"

extern __IO uint32_t g_sys_ms;

#define ALARM_CHECK_PERIOD_MS 1000
static uint32_t alarm_last_check_ms = 0;
static uint8_t ch1_alarm_latched = 0;
static uint8_t ch0_alarm_latched = 0;

void Alarm_SetMode(uint16_t device_id, uint16_t cmd, uint8_t *payload, uint8_t len)
{
    // 设置告警模式，例如主动上报、被动查询等
    uint8_t mode;

    if (payload == 0 || len != 1)
    {
        Protocol_SendError(device_id);
        return;
    }
    mode = payload[0];

    if (!Param_SetAlarmMode(mode))
    {
        Protocol_SendError(device_id);
        return;
    }

    Protocol_SendOK(device_id, cmd);
}

void Alarm_CheckTask(void)
{
    // 定期检查 CH0/CH1 的值是否超过设定的阈值，如果超过则调用 Alarm_Record 记录告警
    float ch0_value, ch1_value;
    if((g_sys_ms - alarm_last_check_ms) < ALARM_CHECK_PERIOD_MS)
    {
        return;
    }
    alarm_last_check_ms = g_sys_ms;

    ch0_value = Sample_GetCH0();
    ch1_value = Sample_GetCH1();

    //CH0超阈值判断
    if(ch0_value > g_system_params.ch0_threshold)
    {
        if(!ch0_alarm_latched)
        {
            ch0_alarm_latched = 1;
            Alarm_Record(0, g_system_params.ch0_threshold, ch0_value);

            //主动告警，则立即上报
            if(g_system_params.alarm_mode == ALARM_MODE_ACTIVE)
            {
                uint8_t buf[256];
                uint16_t len;

                len = AlarmFlash_BuildString(buf, sizeof(buf));
                USART485_Send(buf, len);
            }
        }
    }
    else
    {
        ch0_alarm_latched = 0;
    }

    //CH1超阈值判断
    if(ch1_value > g_system_params.ch1_threshold)
    {
        if(!ch1_alarm_latched)
        {
            ch1_alarm_latched = 1;
            Alarm_Record(1, g_system_params.ch1_threshold, ch1_value);

            //主动告警，则立即上报
            if(g_system_params.alarm_mode == ALARM_MODE_ACTIVE)
            {
                uint8_t buf[256];
                uint16_t len;

                len = AlarmFlash_BuildString(buf, sizeof(buf));
                USART485_Send(buf, len);
            }
        }
    }
    else
    {
        ch1_alarm_latched = 0;
    }
}

void Alarm_Record(uint8_t ch, float threshold, float value)
{
    AlarmFlash_Record(ch, threshold, value);
}

//查询最近的十条告警记录
void Alarm_Query(void)
{
    // 查询并返回最近的告警记录，可以通过串口发送给上位机或者显示在 OLED 上
    uint8_t buf[600];
    uint16_t len;

    len = AlarmFlash_BuildString(buf, sizeof(buf));

    if(len > 0)
    {
        USART485_Send(buf, len);
    }
}
void Alarm_Clear(uint16_t device_id, uint16_t cmd)
{
    float ch0_value;
    float ch1_value;

    /* 清除 Flash 里的告警记录 */
    AlarmFlash_Clear();
	
	
    ch0_value = Sample_GetCH0();
    ch1_value = Sample_GetCH1();

    ch0_alarm_latched = (ch0_value > g_system_params.ch0_threshold) ? 1 : 0;
    ch1_alarm_latched = (ch1_value > g_system_params.ch1_threshold) ? 1 : 0;

    /* 给上位机查询 empty 留一个干净窗口 */
    alarm_last_check_ms = g_sys_ms;

    Protocol_SendOK(device_id, cmd);
}

