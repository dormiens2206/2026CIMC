/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006-2018 Christian Walter <cwalter@embedded-solutions.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"
#include "assert.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbrtu.h"
#include "mbframe.h"

#include "mbcrc.h"
#include "mbport.h"

/* ----------------------- Defines ------------------------------------------*/
/* Modbus RTU 串行帧的最小长度：
 * 从站地址1字节 + 功能码至少1字节 + CRC2字节 = 至少4字节
 */
#define MB_SER_PDU_SIZE_MIN     4       /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_MAX     256     /* Modbus RTU 一帧允许的最大长度 */
#define MB_SER_PDU_SIZE_CRC     2       /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0       /* 从站地址在整个RTU串行帧中的偏移量，位于第0字节 */
/* Modbus PDU在整个RTU串行帧中的起始偏移量。
 * 第0字节是从站地址，因此PDU从第1字节开始。
 */
#define MB_SER_PDU_PDU_OFF      1       

/* ----------------------- Type definitions ---------------------------------*/
typedef enum
{
    STATE_RX_INIT,              /*RTU刚启动时初始化状态*/
                                /*此时协议栈等待总线至少静默T3.5s，确认总线空闲 */
    STATE_RX_IDLE,              /*接收器空闲 */
    STATE_RX_RCV,               /*正在接收报文 */
    STATE_RX_ERROR              /*当前帧接收发生错误 */
} eMBRcvState;

//发送状态
typedef enum
{
    STATE_TX_IDLE,              /*当前没有数据需要发送 */
    STATE_TX_XMIT               /*发送器正在传输状态 */
} eMBSndState;

/* ----------------------- Static variables ---------------------------------*/
/* 当前发送状态。
 * volatile：该变量可能在中断和主循环之间被访问。
 */
static volatile eMBSndState eSndState;      
static volatile eMBRcvState eRcvState;      /* 当前接收状态。 */

volatile UCHAR  ucRTUBuf[MB_SER_PDU_SIZE_MAX];      /* Modbus RTU共用收发缓冲区，最大256字节。 */

static volatile UCHAR *pucSndBufferCur;       /* 指向“当前准备发送的字节”的指针。 */     
static volatile USHORT usSndBufferCount;        /* 当前还剩多少字节没有发送。 */   

static volatile USHORT usRcvBufferPos;      /* 当前已经接收到多少字节，同时也是下一个字节写入的位置。 */

/* ----------------------- Start implementation -----------------------------*/
eMBErrorCode
eMBRTUInit( UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    ULONG           usTimerT35_50us;

    ( void )ucSlaveAddress;
    ENTER_CRITICAL_SECTION(  );

    /*
     * Modbus RTU使用8位数据位。
     * 调用移植层初始化串口：
     *
     * ucPort      串口编号
     * ulBaudRate  波特率
     * 8           8位数据位
     * eParity     校验方式
     */
    if( xMBPortSerialInit( ucPort, ulBaudRate, 8, eParity ) != TRUE )
    {
        eStatus = MB_EPORTERR;  /* 串口初始化失败 */
    }
    else
    {
        /*
         * 根据波特率计算Modbus RTU的T3.5。
         *
         * 波特率 > 19200：
         * 按Modbus规范使用固定约1.75ms的T3.5。
         *
         * 波特率 <= 19200：
         * T3.5按照3.5个字符传输时间计算。
         */
        if( ulBaudRate > 19200 )
        {
            usTimerT35_50us = 35;       /* 1750us */
        }
        else
        {
            /*
             * 一个RTU字符这里按11bit估算：
             *
             * 1个字符时间对应的50us计数值：
             *
             * 11 × 20000 / BaudRate
             * = 220000 / BaudRate
             *
             * T3.5 = 3.5 × 字符时间
             *      = 7/2 × 字符时间
             */
            usTimerT35_50us = ( 7UL * 220000UL ) / ( 2UL * ulBaudRate );
        }
        if( xMBPortTimersInit( ( USHORT ) usTimerT35_50us ) != TRUE )
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

void
eMBRTUStart( void )
{
    ENTER_CRITICAL_SECTION(  );
    /*
     * RTU刚启动时，不能直接认为总线空闲。
     *
     * 因为设备启动的一瞬间，总线上可能正有其他设备通信。
     *
     * 因此先进入 STATE_RX_INIT，
     * 打开接收，并启动T3.5定时器。
     *
     * 如果连续T3.5时间都没有收到任何字符，
     * 才说明RS485总线确实处于空闲状态，
     * 此时协议栈才会进入STATE_RX_IDLE。
     */
    eRcvState = STATE_RX_INIT;
    vMBPortSerialEnable( TRUE, FALSE );     /* 打开接收，关闭发送。 */
    vMBPortTimersEnable(  );        /* 开始等待一个完整的T3.5静默时间。 */

    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStop( void )
{
    ENTER_CRITICAL_SECTION(  );
    /* 同时关闭串口接收和发送。 */
    vMBPortSerialEnable( FALSE, FALSE );
    /* 停止RTU帧间隔定时器。 */
    vMBPortTimersDisable(  );
    EXIT_CRITICAL_SECTION(  );
}

/*

30 -- GD30 PHY

*/

eMBErrorCode
eMBRTUReceive( UCHAR * pucRcvAddress, UCHAR ** pucFrame, USHORT * pusLength )
{
    // BOOL            xFrameReceived = FALSE;
    eMBErrorCode    eStatus = MB_ENOERR;

    ENTER_CRITICAL_SECTION(  );
    /* 防止接收缓冲区位置越界。 */
    assert( usRcvBufferPos < MB_SER_PDU_SIZE_MAX );

    /*
     * 判断这一帧是否合法：
     *
     * 1. 总长度至少达到Modbus RTU最小帧长；
     * 2. 对整帧进行CRC16校验，正确结果应为0。
     */
    if( ( usRcvBufferPos >= MB_SER_PDU_SIZE_MIN )
        && ( usMBCRC16( ( UCHAR * ) ucRTUBuf, usRcvBufferPos ) == 0 ) )
    {
        /*
         * 取出第0字节：从站地址。
         *
         * RTU层本身这里只负责把地址交给上层，
         * 是否是发给本机的，由上层协议逻辑判断。
         */
        *pucRcvAddress = ucRTUBuf[MB_SER_PDU_ADDR_OFF];

        /*
         * 计算真正的Modbus PDU长度。
         *
         * 整个RTU帧长度
         * - 1字节从站地址
         * - 2字节CRC
         * = PDU长度
         */
        *pusLength = ( USHORT )( usRcvBufferPos - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC );

        /*
         * 返回PDU起始地址。
         *
         * ucRTUBuf[0] = 从站地址
         * ucRTUBuf[1] = 功能码 ← PDU从这里开始
         */
        *pucFrame = ( UCHAR * ) & ucRTUBuf[MB_SER_PDU_PDU_OFF];
        // xFrameReceived = TRUE;
    }
    else
    {
        eStatus = MB_EIO;
    }

    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUSend( UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usCRC16;

    ENTER_CRITICAL_SECTION(  );

    /*
     * 只有接收器处于空闲状态时才允许发送响应。
     *
     * 如果当前又开始接收其他数据，
     * 说明从站处理上一帧太慢，不能再直接发送响应。
     */
    if( eRcvState == STATE_RX_IDLE )
    {
        /* First byte before the Modbus-PDU is the slave address. */
        pucSndBufferCur = ( UCHAR * ) pucFrame - 1;
        usSndBufferCount = 1;

        /* 在PDU前写入从站地址。 */
        pucSndBufferCur[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
        usSndBufferCount += usLength;        /* 加上原本PDU的长度。 */

        /* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
        usCRC16 = usMBCRC16( ( UCHAR * ) pucSndBufferCur, usSndBufferCount );

        /*
         * Modbus RTU CRC采用低字节在前、高字节在后。
         */
        ucRTUBuf[usSndBufferCount++] = ( UCHAR )( usCRC16 & 0xFF );
        ucRTUBuf[usSndBufferCount++] = ( UCHAR )( usCRC16 >> 8 );

        /*
         * 进入发送状态。
         */
        eSndState = STATE_TX_XMIT;
        //关闭接收、开启发送
        vMBPortSerialEnable( FALSE, TRUE );
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

BOOL
xMBRTUReceiveFSM( void )
{
    BOOL            xTaskNeedSwitch = FALSE;
    UCHAR           ucByte;

    assert( eSndState == STATE_TX_IDLE );

    /* Always read the character. */
    ( void )xMBPortSerialGetByte( ( CHAR * ) & ucByte );
	// printf("eRcvState=%d\r\n", eRcvState);

    switch ( eRcvState )
    {

    case STATE_RX_INIT:
            /*
             * 协议栈刚启动，目前还在判断总线是否空闲。
             *
             * 此时收到的字符不作为有效新帧保存，
             * 只重新启动T3.5。
             *
             * 只有总线连续静默T3.5后，
             * 才真正进入STATE_RX_IDLE。
             */
        vMBPortTimersEnable(  );
        break;

    case STATE_RX_ERROR:
            /*
             * 当前帧已经出现错误。
             *
             * 后续字符不再保存，只不断重新计时。
             * 等总线最终静默T3.5后丢弃整个错误帧，
             * 再恢复到IDLE状态。
             */
        vMBPortTimersEnable(  );
        break;

    case STATE_RX_IDLE:
            /*
             * 当前处于空闲状态，现在收到一个字符，
             * 说明一帧新的Modbus RTU报文开始了。
             */
        usRcvBufferPos = 0;
        ucRTUBuf[usRcvBufferPos++] = ucByte;
        eRcvState = STATE_RX_RCV;

	// printf("eRcvState=%d\r\n", eRcvState);
            /*
             * 启动T3.5定时器。
             *
             * 如果之后继续收到字符，
             * 每收到一个字符都会重新开始计时。
             */
        vMBPortTimersEnable(  );
        break;

        /* We are currently receiving a frame. Reset the timer after
         * every character received. If more than the maximum possible
         * number of bytes in a modbus frame is received the frame is
         * ignored.
         */
    case STATE_RX_RCV:
        if( usRcvBufferPos < MB_SER_PDU_SIZE_MAX )
        {
            ucRTUBuf[usRcvBufferPos++] = ucByte;
			// printf("usRcvBufferPos=%d\r\n", usRcvBufferPos);
        }
        else
        {
            /*
             * 超过Modbus RTU最大帧长度，
             * 当前帧视为错误。
             */
            eRcvState = STATE_RX_ERROR;
        }
        vMBPortTimersEnable(  );
        break;
    }
    return xTaskNeedSwitch;
}

//发送状态机
BOOL
xMBRTUTransmitFSM( void )
{
    BOOL            xNeedPoll = FALSE;

    assert( eRcvState == STATE_RX_IDLE );

    switch ( eSndState )
    {
        /* We should not get a transmitter event if the transmitter is in
         * idle state.  */
    case STATE_TX_IDLE:
            /*
             * 当前没有数据需要发送。
             * 保持接收打开、发送关闭。
             */
        vMBPortSerialEnable( TRUE, FALSE );
        break;

    case STATE_TX_XMIT:
            /*
             * 如果发送缓冲区里还有数据，
             * 就发送当前指针指向的1个字节。
             */
        if( usSndBufferCount != 0 )
        {
            xMBPortSerialPutByte( ( CHAR )*pucSndBufferCur );
            pucSndBufferCur++;  /* next byte in sendbuffer. */
            usSndBufferCount--;
        }
        else
        {
            xNeedPoll = xMBPortEventPost( EV_FRAME_SENT );
            /* Disable transmitter. This prevents another transmit buffer
             * empty interrupt. */
            vMBPortSerialEnable( TRUE, FALSE );
            eSndState = STATE_TX_IDLE;
        }
        break;
    }

    return xNeedPoll;
}

BOOL
xMBRTUTimerT35Expired( void )
{
    BOOL            xNeedPoll = FALSE;

    switch ( eRcvState )
    {
        /* Timer t35 expired. Startup phase is finished. */
    case STATE_RX_INIT:
        xNeedPoll = xMBPortEventPost( EV_READY );
        break;

        /* A frame was received and t35 expired. Notify the listener that
         * a new frame was received. */
    case STATE_RX_RCV:
        xNeedPoll = xMBPortEventPost( EV_FRAME_RECEIVED );
        break;

        /* An error occured while receiving the frame. */
    case STATE_RX_ERROR:
        break;

        /* Function called in an illegal state. */
    default:
        assert( ( eRcvState == STATE_RX_INIT ) ||
                ( eRcvState == STATE_RX_RCV ) || ( eRcvState == STATE_RX_ERROR ) );
    }

    vMBPortTimersDisable(  );
    eRcvState = STATE_RX_IDLE;

    return xNeedPoll;
}
