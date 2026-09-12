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

#ifndef _MB_CONFIG_H
#define _MB_CONFIG_H

#ifdef __cplusplus
PR_BEGIN_EXTERN_C
#endif
/* ----------------------- Defines ------------------------------------------*/
/*! \defgroup modbus_cfg Modbus Configuration
 *
 * Most modules in the protocol stack are completly optional and can be
 * excluded. This is specially important if target resources are very small
 * and program memory space should be saved.<br>
 *
 * All of these settings are available in the file <code>mbconfig.h</code>
 */
/*! \addtogroup modbus_cfg
 *  @{
 */


/* 是否启用 Modbus ASCII 模式。
 * RS485 通信一般使用 RTU，所以这里关闭。
 */
#define MB_ASCII_ENABLED                        (  0 )

/* 是否启用 Modbus RTU 模式。
 * 从站开发优先使用 RTU。
 */
#define MB_RTU_ENABLED                          (  1 )

/* 是否启用 Modbus TCP 模式。
 * 当前主要使用 RS485 + RTU，所以关闭。
 */
#define MB_TCP_ENABLED                          (  0 )

/* Modbus ASCII 模式下的字符超时时间，单位：秒。
 * 因为我们关闭了 ASCII，所以这个配置暂时不会用到。
 */
#define MB_ASCII_TIMEOUT_SEC                    (  1 )

/* ASCII 模式发送前等待时间，单位：ms。*/

#ifndef MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS
#define MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS    ( 0 )
#endif

/* 协议栈最多支持的功能码处理函数数量。
 * 这个值要大于等于当前启用的功能码数量。
 */
#define MB_FUNC_HANDLERS_MAX                    ( 16 )

/* Report Slave ID 功能的缓冲区大小。
 * 只有 MB_FUNC_OTHER_REP_SLAVEID_ENABLED 为 1 时才会使用。
 */
#define MB_FUNC_OTHER_REP_SLAVEID_BUF           ( 32 )

/* 是否启用 Report Slave ID 功能，功能码 0x11。*/
#define MB_FUNC_OTHER_REP_SLAVEID_ENABLED       (  0 )

/* 是否启用读输入寄存器，功能码 0x04。*/
#define MB_FUNC_READ_INPUT_ENABLED              (  1 )

/* 是否启用读保持寄存器，功能码 0x03。*/
#define MB_FUNC_READ_HOLDING_ENABLED            (  1 )

/* 是否启用写单个保持寄存器，功能码 0x06。
 * 线圈一般表示可读写的开关量。
 */
#define MB_FUNC_WRITE_HOLDING_ENABLED           (  1 )

/* 是否启用写多个保持寄存器，功能码 0x10。 */
#define MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED  (  1 )

/* 是否启用读线圈，功能码 0x01。 */
#define MB_FUNC_READ_COILS_ENABLED              (  1 )

/* 是否启用写单个线圈，功能码 0x05 */
#define MB_FUNC_WRITE_COIL_ENABLED              (  1 )

/* 是否启用写多个线圈，功能码 0x0F */
#define MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED    (  1 )

/* 是否启用读离散输入，功能码 0x02 */
#define MB_FUNC_READ_DISCRETE_INPUTS_ENABLED    (  1 )

/* 是否启用读写多个保持寄存器，功能码 0x17 */
#define MB_FUNC_READWRITE_HOLDING_ENABLED       (  0 )

/*! @} */
#ifdef __cplusplus
    PR_END_EXTERN_C
#endif
#endif
