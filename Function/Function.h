#ifndef __FUNCTION_H
#define __FUNCTION_H

#include "HeaderFiles.h"

extern volatile uint8_t g_mb_tx_count;
extern volatile uint8_t g_mb_last_tx_byte;

typedef enum{
	MODE_ASCII = 0,
	MODE_WORK
}SystemRXMode_t;

void System_Init(void);
void System_Hardware_Init(void);
void Init_LED_Stat(void);
void System_Task(void);
void UsrFunction(void);


#endif
