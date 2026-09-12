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

#define TRUE            1
#define FLASE           0
typedef uint8_t BOOL;

typedef enum {
    KEY_NONE = 0,
    KEY1_ON,
    KEY2_ON,
    KEY3_ON
} Key_Event;

void KEY_Init(void);
void KEY_Scan(void);
Key_Event Key_Get_Event(void);


#endif
