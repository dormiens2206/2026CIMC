#ifndef __RTC_HW_H
#define __RTC_HW_H

#include "HeaderFiles.h"

#define RTC_BKP_MAGIC_VALUE   0x30

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}Time_t;

void RTC_Init(void);
void RTC_GetTime(Time_t *time_struct);
uint32_t RTC_SetTime(const Time_t *time_struct);

void RTC_SetWakeup(uint32_t seconds);

#endif

