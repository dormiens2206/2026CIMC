#include "port.h"

#include "mb.h"
#include "mbport.h"

#include "USART_485.h"

static volatile BOOL g_mb_tx_active = FALSE;
static volatile BOOL g_mb_wait_tc   = FALSE;
static volatile uint8_t g_mb_tx_trace[32];
static volatile uint8_t g_mb_tx_trace_len = 0;
volatile uint8_t g_mb_tx_count = 0;
volatile uint8_t g_mb_last_tx_byte = 0;
volatile uint8_t g_mb_stopbits = 1U;
volatile uint8_t g_mb_databits = 8U;

BOOL xMBPortSerialInit(UCHAR ucPort, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    uint8_t parity_reg;

    (void)ucPort;       // 串口号

    parity_reg = (eParity == MB_PAR_ODD) ? 1U : ((eParity == MB_PAR_EVEN) ? 2U : 0U);
    
	USART485_Init();
	usart_baudrate_set(RS485_USART, ulBaudRate);

    USART485_Config(g_mb_databits, g_mb_stopbits, parity_reg);

    //关中断
    usart_interrupt_disable(RS485_USART, USART_INT_RBNE);   
    usart_interrupt_disable(RS485_USART, USART_INT_IDLE);
    usart_interrupt_disable(RS485_USART, USART_INT_TBE);
    usart_interrupt_disable(RS485_USART, USART_INT_TC);

    RS485_RX_ENABLE();

	return TRUE;
}

void vMBPortClose(void)
{
    usart_interrupt_disable(RS485_USART, USART_INT_RBNE);   
    usart_interrupt_disable(RS485_USART, USART_INT_IDLE);
    usart_interrupt_disable(RS485_USART, USART_INT_TBE);
    usart_interrupt_disable(RS485_USART, USART_INT_TC);

    RS485_RX_ENABLE();
}

//FreeModbus 控制串口接收和发送中断
// FreeModbus 控制串口接收和发送中断
void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    /*
     * =========================
     * 进入发送模式
     * =========================
     */
    if(xTxEnable == TRUE)
    {
		g_mb_tx_trace_len = 0;
        /* 发送期间关闭接收中断 */
        usart_interrupt_disable(
            RS485_USART,
            USART_INT_RBNE);

        /* 关闭TC中断，准备新一轮发送 */
        usart_interrupt_disable(
            RS485_USART,
            USART_INT_TC);

        /*
         * 清除上一次发送完成标志。
         * 后面等本帧最后一个字节真正发完，
         * TC会重新置位。
         */
        usart_interrupt_flag_clear(
            RS485_USART,
            USART_INT_FLAG_TC);

        /* 切换RS485为发送状态 */
        RS485_TX_ENABLE();

        g_mb_tx_active = TRUE;
        g_mb_wait_tc = FALSE;

        /* FreeModbus通过TBE逐字节发送 */
        usart_interrupt_enable(
            RS485_USART,
            USART_INT_TBE);

        return;
    }


    /*
     * =========================
     * 不再继续发送数据
     * =========================
     */

    usart_interrupt_disable(
        RS485_USART,
        USART_INT_TBE);


    /*
     * FreeModbus要求恢复接收
     */
    if(xRxEnable == TRUE)
    {
        /*
         * 如果刚刚发送过一帧，
         * 此时不能立即RS485_RX_ENABLE。
         *
         * 最后一个字节可能还在移位寄存器中发送。
         */
        if(g_mb_tx_active == TRUE)
        {
            g_mb_wait_tc = TRUE;

            /*
             * 开启TC中断。
             * 等最后一个停止位真正发完之后，
             * 再在TC ISR里面切回RX。
             */
            usart_interrupt_enable(
                RS485_USART,
                USART_INT_TC);
        }
        else
        {
            /*
             * 没有经历发送过程。
             * 比如FreeModbus刚启动时，
             * 可以直接进入接收模式。
             */
            RS485_RX_ENABLE();

            usart_interrupt_enable(
                RS485_USART,
                USART_INT_RBNE);
        }
    }
    else
    {
        /*
         * RX/TX都关闭，例如协议栈停止。
         */
        usart_interrupt_disable(
            RS485_USART,
            USART_INT_RBNE);

        usart_interrupt_disable(
            RS485_USART,
            USART_INT_TC);

        g_mb_tx_active = FALSE;
        g_mb_wait_tc = FALSE;

        RS485_RX_ENABLE();
    }
}

//FreeModbus 发送 1 个字节
BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    uint8_t data;

    data = (uint8_t)ucByte;

    g_mb_last_tx_byte = data;
    g_mb_tx_count++;

    usart_data_transmit(
        RS485_USART,
        (uint16_t)data);

    return TRUE;
}

//FreeModbus 接收 1 个字节
BOOL xMBPortSerialGetByte(CHAR* pucByte)
{
    //Freemodbus收到字节时，调用函数读取数据
	*pucByte = (CHAR)usart_data_receive(RS485_USART);
	return TRUE;
}

void vMBPortSerialTxCompleteISR(void)
{
    /*
     * 清除TC中断标志
     */
    usart_interrupt_flag_clear(
        RS485_USART,
        USART_INT_FLAG_TC);

    /*
     * TC只需要触发一次
     */
    usart_interrupt_disable(
        RS485_USART,
        USART_INT_TC);

    if(g_mb_wait_tc == TRUE)
    {
        g_mb_wait_tc = FALSE;
        g_mb_tx_active = FALSE;

        /*
         * 到这里代表：
         *
         * 最后一字节
         * + 最后一个停止位
         *
         * 已真正发送完成。
         */
		delay_us(100);

        RS485_RX_ENABLE();

        /* 恢复FreeModbus接收 */
        usart_interrupt_enable(
            RS485_USART,
            USART_INT_RBNE);
    }
}

BOOL xMBPortSerialIsTxIdle(void)
{
    return (g_mb_tx_active == FALSE && g_mb_wait_tc == FALSE)
               ? TRUE : FALSE;
}



