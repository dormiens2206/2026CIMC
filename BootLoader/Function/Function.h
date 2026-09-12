#ifndef __FUNCTION_H
#define __FUNCTION_H

#include "HeaderFiles.h"

void AD3344_reg_Config(uint8_t InputMUX, uint8_t Channel);
float adcdata_to_volt(uint16_t bin);

void System_Init(void);
void System_Hardware_Init(void);
void Init_LED_Stat(void);
void SPI_Flash_Test(void);

void UsrFunction(void);

#endif
