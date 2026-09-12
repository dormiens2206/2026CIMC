#ifndef __LED_H
#define __LED_H

#include "HeaderFiles.h"

#define LED_RCU    RCU_GPIOB
#define LED_PORT   GPIOB

#define LED1_PIN    GPIO_PIN_13
#define LED2_PIN    GPIO_PIN_12
#define LED3_PIN    GPIO_PIN_14
#define LED4_PIN    GPIO_PIN_15

#define LED1_OFF()  gpio_bit_reset(LED_PORT, LED1_PIN)
#define LED1_ON()   gpio_bit_set(LED_PORT, LED1_PIN)

#define LED2_OFF()  gpio_bit_reset(LED_PORT, LED2_PIN)
#define LED2_ON()   gpio_bit_set(LED_PORT, LED2_PIN)

#define LED3_OFF()  gpio_bit_reset(LED_PORT, LED3_PIN)
#define LED3_ON()   gpio_bit_set(LED_PORT, LED3_PIN)

#define LED4_OFF()  gpio_bit_reset(LED_PORT, LED4_PIN)
#define LED4_ON()   gpio_bit_set(LED_PORT, LED4_PIN)

#define LED1_TOGGLE()  gpio_bit_toggle(LED_PORT, LED1_PIN)
#define LED2_TOGGLE()  gpio_bit_toggle(LED_PORT, LED2_PIN)
#define LED3_TOGGLE()  gpio_bit_toggle(LED_PORT, LED3_PIN)
#define LED4_TOGGLE()  gpio_bit_toggle(LED_PORT, LED4_PIN)

extern uint8_t LED_Stat;

void LED_Init(void);
void LED_STATE(void);
void LED_Status_Update(uint8_t mode);

#endif
