#ifndef __APP_TASK_H
#define __APP_TASK_H

#include "HeaderFiles.h"

void App_StartAutoReport(uint16_t device_id, uint16_t cmd);
void App_StopAutoReport(uint16_t device_id, uint16_t cmd);
void App_AutoReportTask(void);
uint8_t App_AutoReporting(void);

void App_RequestSleep(uint16_t device_id, uint16_t cmd);
void App_SleepTask(void);

void App_ResetRequest(uint16_t device_id, uint16_t cmd);
void App_ResetTask(void);

void App_RequestBootloader(uint16_t device_id, uint16_t cmd);
void App_BootloaderTask(void);

#endif
