#include "USART_485.h"
#include "mb.h"
#include "mbport.h"
#include "project_config.h"

// 发送相关变量
static uint8_t Buf_Send[600];
static uint16_t rs485_send_len = 0;
static uint16_t rs485_send_index = 0;
static volatile uint8_t rs485_send_finish = 1; //发送完成标志

// 接收相关变量
static uint8_t rs485_recv_buf[600] = { 0 };
static uint16_t rs485_recv_len = 0;
static uint8_t rs485_recv_real_buf[600] = { 0 };
static uint16_t rs485_recv_real_len = 0;
static volatile uint8_t rs485_recv_flag = 0;

extern SystemRXMode_t  current_mode;

void USART485_Init(void)
{
    //开启中断并设置优先级
    nvic_irq_enable(RS485_USART_IRQn, 2, 0);

    //使能时钟
    rcu_periph_clock_enable(RS485_USART_RCU);
    rcu_periph_clock_enable(RS485_TX_RCU);
    rcu_periph_clock_enable(RS485_RX_RCU);
    rcu_periph_clock_enable(RS485_DE_RCU);

    gpio_mode_set(RS485_DE_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_DE_PIN);
    gpio_output_options_set(RS485_DE_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_DE_PIN);

    //配置TX，推挽输出
    gpio_af_set(RS485_TX_PORT, RS485_TX_AF, RS485_TX_PIN);
    gpio_mode_set(RS485_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, RS485_TX_PIN);
    gpio_output_options_set(RS485_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_TX_PIN);

    //配置RX，上拉输入
    gpio_af_set(RS485_RX_PORT, RS485_RX_AF, RS485_RX_PIN);
    gpio_mode_set(RS485_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, RS485_RX_PIN);

    usart_deinit(RS485_USART);
		
		//波特率设置
		uint32_t baudrate;
		baudrate = Param_BaudCodeToBaudrate(g_system_params.baud_code);
		usart_baudrate_set(RS485_USART, baudrate);

    //同时使能发送和接收
    usart_transmit_config(RS485_USART, USART_TRANSMIT_ENABLE);
    usart_receive_config(RS485_USART, USART_RECEIVE_ENABLE);
    
    //开启串口总开关
    usart_enable(RS485_USART);

    //启动串口中断源
    usart_interrupt_enable(RS485_USART, USART_INT_RBNE); // 接收缓冲区非空中断
    usart_interrupt_enable(RS485_USART, USART_INT_IDLE); // 空闲总线中断

    RS485_RX_ENABLE(); //默认进入接收模式
}

void USART485_Config(uint8_t databits, uint8_t stopbits, uint8_t parity)
{
    if(databits == 9U)
    {
        usart_word_length_set(RS485_USART, USART_WL_9BIT);
    }
    else
    {
        usart_word_length_set(RS485_USART, USART_WL_8BIT);
    }

    if(stopbits == 2U)
    {
        usart_stop_bit_set(RS485_USART, USART_STB_2BIT);
    }
    else
    {
        usart_stop_bit_set(RS485_USART, USART_STB_1BIT);
    }

    if(parity == 1U)
    {
        usart_parity_set(RS485_USART, USART_PM_ODD);
    }
    else if(parity == 2U)
    {
        usart_parity_set(RS485_USART, USART_PM_EVEN);
    }
    else
    {
        usart_parity_set(RS485_USART, USART_PM_NONE);
    }
}

void USART485_Send(const uint8_t *buf, uint16_t len)
{
    if(len > sizeof(Buf_Send))
    {
        return;
    }

    while (!rs485_send_finish)
    {
        //等待上一次发送完成，避免数据冲突
    }
    rs485_send_finish = 0; //重置发送完成标志

    //将要发的数据打包拷入发送缓冲区
    memcpy(Buf_Send, buf, len);
    rs485_send_len = len;
    rs485_send_index = 0;

    RS485_TX_ENABLE(); //切换到发送模式

    //关闭接收相关中断，防止发送过程中被接收中断打断
    usart_interrupt_disable(RS485_USART, USART_INT_RBNE);
    usart_interrupt_disable(RS485_USART, USART_INT_IDLE); 

    //开启TBE（发送缓冲区空）中断
    usart_interrupt_enable(RS485_USART, USART_INT_TBE);

}

uint16_t USART485_Read(uint8_t *buf, uint16_t max_len)
{
    uint16_t read_len = 0;

    if(rs485_recv_flag)
    {
        read_len = (rs485_recv_real_len < max_len) ? rs485_recv_real_len : max_len;
        memcpy(buf, rs485_recv_real_buf, read_len);
        rs485_recv_flag = 0;
    }

    return read_len;
}

void USART1_IRQHandler(void)
{
    switch (current_mode)
    {
        case MODE_ASCII:  
        {
            //处理接收中断
        if(usart_interrupt_flag_get(RS485_USART,USART_INT_FLAG_RBNE) != RESET)
        {
            if(rs485_recv_len < sizeof(rs485_recv_buf))
            {
                rs485_recv_buf[rs485_recv_len++] = usart_data_receive(RS485_USART);
            }
            else
            {
                rs485_recv_len = 0;
                rs485_recv_flag = 0;
            }
        }

        //空闲中断，表示一包数据接收完成
        if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_IDLE) != RESET)
        {
            usart_data_receive(RS485_USART);

            if(rs485_recv_len != 0)
            {
                memcpy(rs485_recv_real_buf, rs485_recv_buf, rs485_recv_len);
                rs485_recv_real_len = rs485_recv_len;
                rs485_recv_len = 0;
                rs485_recv_flag = 1;
            }
        }

        //发送中断，发送数据寄存器空 (TBE)
        if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TBE) != RESET &&rs485_send_finish == 0)
        {
            if(rs485_send_index < rs485_send_len)
            {
                usart_data_transmit(RS485_USART, Buf_Send[rs485_send_index++]);
            }
            else
            {
                usart_interrupt_disable(RS485_USART, USART_INT_TBE);
                usart_interrupt_enable(RS485_USART, USART_INT_TC);
            }
        }

        //处理发送完成中断
        if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TC) != RESET &&rs485_send_finish == 0)
        {
            usart_interrupt_flag_clear(RS485_USART, USART_INT_FLAG_TC);
            usart_interrupt_disable(RS485_USART, USART_INT_TC);

            RS485_RX_ENABLE();
            rs485_send_finish = 1;

            usart_interrupt_enable(RS485_USART, USART_INT_RBNE);
            usart_interrupt_enable(RS485_USART, USART_INT_IDLE);
        }
    }
    break;

    case MODE_WORK:
    {
        //处理接收中断
        if(usart_interrupt_flag_get(RS485_USART,USART_INT_FLAG_RBNE) != RESET)
        {
            pxMBFrameCBByteReceived();      //调用Modbus接收回调函数
        }
        //发送中断
        if(usart_interrupt_flag_get(RS485_USART,USART_INT_FLAG_TBE) != RESET)
        {
            pxMBFrameCBTransmitterEmpty();  //调用Modbus发送回调函数
        }
        //发送完成中断
        if(usart_interrupt_flag_get(RS485_USART,USART_INT_FLAG_TC)!= RESET)
        {
        
        }
    vMBPortSerialTxCompleteISR();   //调用Modbus发送完成回调函数
    }
    break;
    
    default:
        current_mode = MODE_ASCII;
        break;           
}
       
}

//睡眠用的：先回复 OK，等 OK 发完，再进睡眠。
uint8_t USART485_IsSendFinish(void)
{
    return rs485_send_finish;
}


/*
void USART1_IRQHandler(void)
{
    //LED4_TOGGLE();
    //处理接收中断
    if(usart_interrupt_flag_get(RS485_USART,USART_INT_FLAG_RBNE) != RESET)
    {
        if(rs485_recv_len < sizeof(rs485_recv_buf))
        {
            rs485_recv_buf[rs485_recv_len++] = usart_data_receive(RS485_USART);
        }
        else
        {
            rs485_recv_len = 0;
            rs485_recv_flag = 0;
        }
    }

    //空闲中断，表示一包数据接收完成
    if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_IDLE) != RESET)
    {
        usart_data_receive(RS485_USART);

        if(rs485_recv_len != 0)
        {
            memcpy(rs485_recv_real_buf, rs485_recv_buf, rs485_recv_len);
            rs485_recv_real_len = rs485_recv_len;
            rs485_recv_len = 0;
            rs485_recv_flag = 1;
        }
    }

    //发送中断，发送数据寄存器空 (TBE)
    if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TBE) != RESET &&rs485_send_finish == 0)
    {
        if(rs485_send_index < rs485_send_len)
        {
            usart_data_transmit(RS485_USART, Buf_Send[rs485_send_index++]);
        }
        else
        {
            usart_interrupt_disable(RS485_USART, USART_INT_TBE);
            usart_interrupt_enable(RS485_USART, USART_INT_TC);
        }
    }

    //处理发送完成中断
    if(usart_interrupt_flag_get(RS485_USART, USART_INT_FLAG_TC) != RESET &&rs485_send_finish == 0)
    {
        usart_interrupt_flag_clear(RS485_USART, USART_INT_FLAG_TC);
        usart_interrupt_disable(RS485_USART, USART_INT_TC);

        RS485_RX_ENABLE();
        rs485_send_finish = 1;

        usart_interrupt_enable(RS485_USART, USART_INT_RBNE);
        usart_interrupt_enable(RS485_USART, USART_INT_IDLE);
    }
}
*/

