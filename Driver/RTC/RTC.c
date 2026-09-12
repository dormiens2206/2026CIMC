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

//将rtc获取的时间转换成32位的秒数
uint32_t TimeStruct_To_Timestamp(Time_t *time)
{
    const uint16_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint32_t days = 0;
    uint16_t year, month;
    
    //累加1970年到当前年份的前一年的总天数
    for(year = 1970; year < time->year; year++) 
    {
        //闰年
        if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) days += 366;
        else days += 365;
    }

    //累加月份的天数
    for(month = 1; month < time->month; month++) 
    {
        days += month_days[month - 1];
        //如果是闰年且过了 2 月，天数加 1
        if(month == 2 && ((time->year % 4 == 0 && time->year % 100 != 0) || (time->year % 400 == 0))) {
            days += 1;
        }
    }
    //累加当月的天数
    days += time->date - 1;
    
    //将总天数换算成秒
    return (days * 86400) + (time->hour * 3600) + (time->minute * 60) + time->second;
}

void Timestamp_To_TimeStruct(uint32_t timestamp, Time_t *time)
{
    uint32_t days = timestamp / 86400;
    uint32_t seconds_in_day = timestamp % 86400;
    time->hour = seconds_in_day / 3600;
    time->minute = (seconds_in_day % 3600) / 60;
    time->second = seconds_in_day % 60;

    uint16_t year = 1970;
    uint16_t month = 1;
    uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    time->week = (days + 4) % 7; //1970-01-01是星期四
    if(time->week == 0) time->week = 7; //调整为1-7表示周一到周日

    while(1)
    {
        uint16_t days_in_year = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0) ? 366 : 365;
        if(days >= days_in_year) 
        {
            days -= days_in_year;
            year++;
        } 
        else 
            break;
    }
    time->year = year;

    if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) month_days[1] = 29;

    while(days >= month_days[month - 1]) 
    {
        days -= month_days[month - 1];
        month++;
    }
    time->month = month;

    time->date = days + 1;
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
    uint32_t cnt;

    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    /*
     * 先关掉上一次 wakeup。
     * 不关就重新写计数器，可能写不进去，或者旧标志还在。
     */
    rtc_wakeup_disable();
    rtc_interrupt_disable(RTC_INT_WAKEUP);

    /*
     * 清 RTC wakeup 标志和 EXTI22 标志。
     * RTC wakeup 是通过 EXTI_22 唤醒内核的。
     */
    rtc_flag_clear(RTC_FLAG_WT);
    exti_interrupt_flag_clear(EXTI_22);

    /*
     * LXTAL = 32768Hz。
     * WAKEUP_RTCCK_DIV16 后是 2048Hz。
     * 所以 1 秒 = 2048 个计数。
     */
    rtc_wakeup_clock_set(WAKEUP_RTCCK_DIV16);

    cnt = seconds * 2048U;
    if(cnt > 0U)
    {
        cnt -= 1U;
    }

    if(cnt > 0xFFFFU)
    {
        cnt = 0xFFFFU;
    }

    rtc_wakeup_timer_set((uint16_t)cnt);

    /*
     * 关键补丁：把 RTC wakeup 接到 EXTI22，再打开 RTC_WKUP_IRQn。
     * 没有这几句，RTC 到时间了，CPU 也不一定能从 WFI 醒来。
     */
    exti_init(EXTI_22, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    exti_interrupt_flag_clear(EXTI_22);

    nvic_irq_enable(RTC_WKUP_IRQn, 1, 0);

    rtc_interrupt_enable(RTC_INT_WAKEUP);
    rtc_wakeup_enable();
}
uint32_t RTC_GetUnixTime(void)
{
    Time_t current_time;
    RTC_GetTime(&current_time);
    return TimeStruct_To_Timestamp(&current_time);
}

void RTC_SetUnixTime(uint32_t unix_time)
{
    Time_t time_struct;
    Timestamp_To_TimeStruct(unix_time, &time_struct);
    RTC_SetTime(&time_struct);
}

void RTC_SetWakeup_10s(void)
{
    RTC_SetWakeup(10);
}
void RTC_StopWakeup(void)
{
    rtc_wakeup_disable();
    rtc_interrupt_disable(RTC_INT_WAKEUP);

    rtc_flag_clear(RTC_FLAG_WT);
    exti_interrupt_flag_clear(EXTI_22);
}
