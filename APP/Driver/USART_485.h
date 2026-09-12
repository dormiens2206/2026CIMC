#ifndef __USART485_H
#define __USART485_H

#include "HeaderFiles.h"

#define RS485_USART         USART1
#define RS485_USART_RCU     RCU_USART1
#define RS485_USART_IRQn    USART1_IRQn

#define RS485_TX_RCU        RCU_GPIOD
#define RS485_TX_PORT       GPIOD
#define RS485_TX_PIN        GPIO_PIN_5
#define RS485_TX_AF         GPIO_AF_7

#define RS485_RX_RCU        RCU_GPIOD
#define RS485_RX_PORT       GPIOD
#define RS485_RX_PIN        GPIO_PIN_6
#define RS485_RX_AF         GPIO_AF_7

#define RS485_DE_RCU        RCU_GPIOE
#define RS485_DE_PORT       GPIOE
#define RS485_DE_PIN        GPIO_PIN_8

#define RS485_TX_ENABLE()   gpio_bit_set(RS485_DE_PORT, RS485_DE_PIN)
#define RS485_RX_ENABLE()   gpio_bit_reset(RS485_DE_PORT, RS485_DE_PIN)

void USART485_Init(void);
void USART485_Send(const uint8_t *buf, uint16_t len);
uint16_t USART485_Read(uint8_t *buf, uint16_t max_len);
void USART1_IRQHandler(void);
void USART485_Config(uint8_t databits, uint8_t stopbits, uint8_t parity);

uint8_t USART485_IsSendFinish(void);

#endif
