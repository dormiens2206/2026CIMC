#ifndef __SPI_PORT_H
#define __SPI_PORT_H

#include "HeaderFiles.h"

#define SPI_SET_CS()  gpio_bit_set(GPIOE, GPIO_PIN_10)
#define SPI_CLR_CS()  gpio_bit_reset(GPIOE, GPIO_PIN_10)

/* initialize SPI0 */
void ad3344_spi_init(void);
/* SPI transmit and receive 16 bit data */
uint16_t ad3344_spi_txrx16bit(uint16_t tx_byte);

#endif
