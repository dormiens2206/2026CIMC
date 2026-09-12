#ifndef __KEY_H
#define __KEY_H

#include "HeaderFiles.h"

#define KEY_RCU     RCU_GPIOA
#define KEY_PORT    GPIOA

#define KEY1_PIN    GPIO_PIN_7
#define KEY2_PIN    GPIO_PIN_6
#define KEY3_PIN    GPIO_PIN_5
#define KEY4_PIN    GPIO_PIN_4

#define KEY_WAKUP_RCU        RCU_GPIOA
#define KEY_WAKUP_PORT       GPIOA
#define KEY_WAKUP_PIN        GPIO_PIN_0 

#define KEY_CLK     KEY_RCU

typedef enum {
    KEY_NONE = 0,
    WKUP_SHORT,
    WKUP_LONG,
    KEY1_SHORT,
    KEY1_LONG,
    KEY2_SHORT,
    KEY2_LONG,
    KEY3_SHORT,
    KEY3_LONG,
    KEY4_SHORT,
    KEY4_LONG,
} Key_Event;

void KEY_Init(void);
void KEY_Scan(void);
Key_Event Key_Get_Event(void);

#endif
