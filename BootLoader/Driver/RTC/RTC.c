#include "RTC.h"

#define DEC_TO_BCD(x)   (uint8_t)((((x) / 10U) << 4) | ((x) % 10U))
#define BCD_TO_DEC(x)   (uint8_t)((((x) >> 4) * 10U) + ((x) & 0x0FU))

static void RTC_ConfigClock(void);

void RTC_Init(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    if(RTC_BKP0 != RTC_BKP_MAGIC_VALUE) {
        RTC_ConfigClock();

        Time_t default_time = {2026, 1, 1, 4, 0, 0, 0};
        RTC_SetTime(&default_time);

        RTC_BKP0 = RTC_BKP_MAGIC_VALUE;
    } else {
        rcu_periph_clock_enable(RCU_RTC);
        rtc_register_sync_wait();
    }

    rcu_all_reset_flag_clear();
}

static void RTC_ConfigClock(void)
{
    rcu_osci_on(RCU_LXTAL);
    while(!rcu_osci_stab_wait(RCU_LXTAL));

    rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

void RTC_GetTime(Time_t *time_struct)
{
    rtc_parameter_struct rtc_time;

    if(time_struct == NULL) {
        return;
    }

    rtc_register_sync_wait();
    rtc_current_time_get(&rtc_time);

    time_struct->year = (uint16_t)(2000U + BCD_TO_DEC(rtc_time.year));
    time_struct->month = BCD_TO_DEC(rtc_time.month);
    time_struct->date = BCD_TO_DEC(rtc_time.date);
    time_struct->week = rtc_time.day_of_week;
    time_struct->hour = BCD_TO_DEC(rtc_time.hour);
    time_struct->minute = BCD_TO_DEC(rtc_time.minute);
    time_struct->second = BCD_TO_DEC(rtc_time.second);
}

uint32_t RTC_SetTime(const Time_t *time_struct)
{
    rtc_parameter_struct rtc_time;

    if(time_struct == NULL) {
        return ERROR;
    }

    rtc_time.factor_asyn = 0x7FU;
    rtc_time.factor_syn = 0xFFU;
    rtc_time.year = DEC_TO_BCD((uint8_t)(time_struct->year - 2000U));
    rtc_time.month = DEC_TO_BCD(time_struct->month);
    rtc_time.date = DEC_TO_BCD(time_struct->date);
    rtc_time.day_of_week = time_struct->week;
    rtc_time.hour = DEC_TO_BCD(time_struct->hour);
    rtc_time.minute = DEC_TO_BCD(time_struct->minute);
    rtc_time.second = DEC_TO_BCD(time_struct->second);
    rtc_time.display_format = RTC_24HOUR;
    rtc_time.am_pm = RTC_AM;

    if(rtc_init(&rtc_time) == ERROR) {
        return ERROR;
    }

    return SUCCESS;
}

void RTC_SetWakeup(uint32_t seconds)
{
    rtc_wakeup_clock_set(WAKEUP_RTCCK_DIV16);
    rtc_wakeup_timer_set((uint16_t)seconds);
    rtc_wakeup_enable();
}
