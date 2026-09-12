#ifndef __PMU_LP_H
#define __PMU_LP_H

#include "HeaderFiles.h"

#define PMU_LP_RCU          RCU_PMU

#define PMU_WKUP_RCU        RCU_GPIOA
#define PMU_WKUP_PORT       GPIOA
#define PMU_WKUP_PIN        GPIO_PIN_0
#define PMU_WKUP_EXTI_LINE  EXTI_0
#define PMU_WKUP_EXTI_IRQn  EXTI0_IRQn

void PMU_LP_Init(void);
void PMU_Enter_Sleep(void);
void PMU_Enter_DeepSleep(void);
void PMU_Exit_LowPower(void);

#endif
