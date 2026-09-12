/*!
    \file    spi_port.c
    \brief   SPI functions port on platform for the gd30ad3344
    
    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "spi_port.h"

/*!
    \brief      initialize SPI0
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_spi_init()
{ 
    //使能时钟并配置GPIO
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_SPI3);

    gpio_af_set(GPIOE, GPIO_AF_5, GPIO_PIN_12);
    gpio_af_set(GPIOE, GPIO_AF_5, GPIO_PIN_13 | GPIO_PIN_14);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14);

    //配置SPI0的CS引脚，推挽输出
    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_10);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
    
    SPI_SET_CS();

    //配置SPI0参数
    spi_parameter_struct spi_init_struct;
    spi_i2s_deinit(SPI3);
    spi_struct_para_init(&spi_init_struct);

    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_16BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_32;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI3, &spi_init_struct);

    spi_enable(SPI3);
    
}

/*!
    \brief      SPI transmit and receive 16 bit data
    \param[in]  tx_byte: the data need to be transmit
    \param[out] none
    \retval     the data received from slave
*/
//uint16_t ad3344_spi_txrx16bit(uint16_t tx_byte)
//{
//    while(SET == spi_i2s_flag_get(SPI3, SPI_FLAG_RBNE)) {
//        (void)spi_i2s_data_receive(SPI3);
//    }

//    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_TBE));
//    
//    /*!< Send byte through the SPI0 peripheral */
//    spi_i2s_data_transmit(SPI3, tx_byte);
//    
//    /*!< Wait to receive a byte */
//    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_RBNE));
//    
//    /*!< Return the byte read from the SPI bus */
//    return spi_i2s_data_receive(SPI3);
//}
uint16_t ad3344_spi_txrx16bit(uint16_t tx_byte)
{
    uint32_t timeout;

    timeout = 200000U;
    while(SET == spi_i2s_flag_get(SPI3, SPI_FLAG_RBNE))
    {
        (void)spi_i2s_data_receive(SPI3);
        if(timeout-- == 0)
        {
            return 0xFFFF;
        }
    }

    timeout = 200000U;
    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_TBE))
    {
        if(timeout-- == 0)
        {
            return 0xFFFF;
        }
    }

    spi_i2s_data_transmit(SPI3, tx_byte);

    timeout = 200000U;
    while(RESET == spi_i2s_flag_get(SPI3, SPI_FLAG_RBNE))
    {
        if(timeout-- == 0)
        {
            return 0xFFFF;
        }
    }

    return spi_i2s_data_receive(SPI3);
}
