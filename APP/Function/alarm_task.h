#ifndef __ALARM_TASK_H
#define __ALARM_TASK_H

#include "HeaderFiles.h"

void Alarm_SetMode(uint16_t device_id, uint16_t cmd, uint8_t *payload, uint8_t len);
void Alarm_CheckTask(void);
void Alarm_Record(uint8_t ch, float threshold, float value);
void Alarm_Query(void);
void Alarm_Clear(uint16_t device_id, uint16_t cmd);

#endif
