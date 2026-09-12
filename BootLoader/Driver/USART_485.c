#include "USART_485.h"
#include <string.h>

#define RS485_RX_BUF_SIZE 8192



// 发送相关变量
static uint8_t Buf_Send[128];
static volatile uint16_t rs485_send_len = 0;
static volatile uint16_t rs485_send_index = 0;
static volatile uint8_t rs485_send_finish = 1; //发送完成标志

// 接收相关变量
static uint8_t rs485_recv_buf[RS485_RX_BUF_SIZE] = { 0 };
static volatile uint16_t rs485_recv_len = 0;
static uint8_t rs485_recv_real_buf[RS485_RX_BUF_SIZE] = { 0 };
static volatile uint16_t rs485_recv_real_len = 0;
static volatile uint8_t rs485_recv_flag = 0;


//#1.串口初始化函数
void USART485_Init(void)
{
	USART485_Init_Baud(19200U);
}

void USART485_Init_Baud(uint32_t baudrate)
{
	nvic_irq_enable(RS485_USART_IRQn, 2, 0);

	//开启时钟
	rcu_periph_clock_enable(RS485_USART_RCU);
	rcu_periph_clock_enable(RS485_TX_RCU);
	rcu_periph_clock_enable(RS485_RX_RCU);
	rcu_periph_clock_enable(RS485_DE_RCU);

	gpio_mode_set(RS485_DE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_DE_PIN);
	gpio_output_options_set(RS485_DE_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_DE_PIN);

	gpio_af_set(RS485_TX_PORT, RS485_TX_AF, RS485_TX_PIN);
	gpio_mode_set(RS485_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, RS485_TX_PIN);
	gpio_output_options_set(RS485_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_TX_PIN);

	gpio_af_set(RS485_RX_PORT, RS485_RX_AF, RS485_RX_PIN);
	gpio_mode_set(RS485_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, RS485_RX_PIN);

	usart_deinit(RS485_USART);
	usart_baudrate_set(RS485_USART, baudrate);

	//串口初始化 8bit数据位，1bit停止位，无校验位
	usart_word_length_set(RS485_USART, USART_WL_8BIT);
	usart_stop_bit_set(RS485_USART, USART_STB_1BIT);
	usart_parity_config(RS485_USART, USART_PM_NONE);
	usart_hardware_flow_rts_config(RS485_USART, USART_RTS_DISABLE);
	usart_hardware_flow_cts_config(RS485_USART, USART_CTS_DISABLE);

	usart_transmit_config(RS485_USART, USART_TRANSMIT_ENABLE);
	usart_receive_config(RS485_USART, USART_RECEIVE_ENABLE);

	usart_enable(RS485_USART);

	usart_interrupt_enable(RS485_USART, USART_INT_RBNE);
	usart_interrupt_enable(RS485_USART, USART_INT_IDLE);

	RS485_RX_ENABLE();
}

//#2.发送数据函数
void USART485_Send(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if((buf == 0) || (len == 0U))
    {
        return;
    }

    /*
     * Bootloader 里不用中断发送，直接轮询发送最稳。
     */
    rs485_send_finish = 0U;

    usart_interrupt_disable(RS485_USART, USART_INT_TBE);
    usart_interrupt_disable(RS485_USART, USART_INT_TC);

    /*
     * 发送期间关闭接收中断，避免把自己发的数据或毛刺收到缓冲区。
     */
    usart_interrupt_disable(RS485_USART, USART_INT_RBNE);
    usart_interrupt_disable(RS485_USART, USART_INT_IDLE);

    rs485_recv_len = 0U;
    rs485_recv_real_len = 0U;
    rs485_recv_flag = 0U;

    RS485_TX_ENABLE();
    delay_1ms(1);

    for(i = 0U; i < len; i++)
    {
        while(usart_flag_get(RS485_USART, USART_FLAG_TBE) == RESET)
        {
        }

        usart_data_transmit(RS485_USART, buf[i]);
    }

    /*
     * 等最后一个字节的停止位真正发完。
     */
    while(usart_flag_get(RS485_USART, USART_FLAG_TC) == RESET)
    {
    }

    usart_flag_clear(RS485_USART, USART_FLAG_TC);

    delay_1ms(1);
    RS485_RX_ENABLE();

    rs485_send_finish = 1U;

    usart_interrupt_enable(RS485_USART, USART_INT_RBNE);
    usart_interrupt_enable(RS485_USART, USART_INT_IDLE);
}

//发送字符串函数
void USART485_SendString(const char *str)
{
 if (str == 0)
	{
			return;
	}

	USART485_Send((const uint8_t *)str, strlen(str));
}


//接收数据函数
uint16_t USART485_Read(uint8_t *buf, uint16_t max_len)
{
    uint16_t len;

    if((buf == 0) || (max_len == 0U))
    {
        return 0U;
    }

    if(rs485_recv_flag == 0U)
    {
        return 0U;
    }

    __disable_irq();

    len = rs485_recv_real_len;

    if(len > max_len)
    {
        len = max_len;
    }

    if(len > 0U)
    {
        memcpy(buf, rs485_recv_real_buf, len);
    }

    rs485_recv_real_len = 0U;
    rs485_recv_flag = 0U;

    __enable_irq();

    return len;
}
uint8_t USART485_IsSendFinish(void)
{
    return rs485_send_finish;
}

void USART485_ClearRx(void)
{
    __disable_irq();

    rs485_recv_len = 0;
    rs485_recv_real_len = 0;
    rs485_recv_flag = 0;

    memset(rs485_recv_buf, 0, sizeof(rs485_recv_buf));
    memset(rs485_recv_real_buf, 0, sizeof(rs485_recv_real_buf));

    __enable_irq();
}

//中断处理函数
void USART1_IRQHandler(void)
{
    uint8_t ch;

    /* 接收中断：收到 1 个字节 */
    if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_RBNE) != RESET)
    {
        /*
         * 必须先读数据寄存器。
         * 不管缓冲区有没有满，都要读，否则 RBNE 标志可能一直不清。
         */
        ch = (uint8_t)usart_data_receive(RS485_USART);

        if(rs485_recv_len < sizeof(rs485_recv_buf))
        {
            rs485_recv_buf[rs485_recv_len++] = ch;
        }
        else
        {
            /*
             * 缓冲区溢出，丢弃这一包。
             * 不要继续写，否则会越界。
             */
            rs485_recv_len = 0;
        }
    }

    /* 空闲中断：表示一包数据接收完成 */
    if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_IDLE) != RESET)
    {
        /*
         * 清 IDLE 标志。
         * GD32/STM32 串口 IDLE 通常需要读状态再读数据。
         * 如果你工程里没有 USART_STAT/USART_DATA 宏，就保留 usart_data_receive 也行。
         */
        usart_data_receive(RS485_USART);

        if(rs485_recv_len != 0)
        {
            memcpy(rs485_recv_real_buf, rs485_recv_buf, rs485_recv_len);
            rs485_recv_real_len = rs485_recv_len;

            rs485_recv_len = 0;
            rs485_recv_flag = 1;
        }
    }

    /* 发送数据寄存器空：TBE */
    if((usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TBE) != RESET) &&
       (rs485_send_finish == 0))
    {
        if(rs485_send_index < rs485_send_len)
        {
            usart_data_transmit(RS485_USART, Buf_Send[rs485_send_index]);
            rs485_send_index++;
        }
        else
        {
            /*
             * 所有字节已经写入发送数据寄存器。
             * 但最后一个字节可能还没真正从 TX 引脚发完。
             * 所以关 TBE，开 TC，等真正发送完成。
             */
            usart_interrupt_disable(RS485_USART, USART_INT_TBE);
            usart_interrupt_enable(RS485_USART, USART_INT_TC);
        }
    }

    /* 发送完成：TC，最后一个停止位也已经发完 */
    if((usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TC) != RESET) &&
       (rs485_send_finish == 0))
    {
        usart_interrupt_flag_clear(RS485_USART, USART_INT_FLAG_TC);
        usart_interrupt_disable(RS485_USART, USART_INT_TC);

        /*
         * TC 已经表示物理发送完成，这时再切回接收。
         * 不要在中断里 delay_1ms，ISR 里尽量别延时。
         */
        RS485_RX_ENABLE();

        rs485_send_finish = 1;

        /*
         * 清掉发送期间可能产生的接收残留。
         */
        rs485_recv_len = 0;
        rs485_recv_real_len = 0;
        rs485_recv_flag = 0;

        usart_interrupt_enable(RS485_USART, USART_INT_RBNE);
        usart_interrupt_enable(RS485_USART, USART_INT_IDLE);
    }
}

