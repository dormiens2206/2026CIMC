/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

 /* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "timer.h"


/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortTimersInit(USHORT usTim1Timerout50us)
{
	//usTim1Timerout50us的单位是50us
    uint32_t timeout_us;
    timeout_us = usTim1Timerout50us * 50;
	RS485_Timer_Init(timeout_us);
	return TRUE;
}


inline void
vMBPortTimersEnable()
{
	//每次启动定时器前先关闭，清零计数值，清楚中断标志
    //保证每次从0开始计时
	timer_disable(TIMER6);
	timer_counter_value_config(TIMER6 , 0);
	timer_interrupt_flag_clear(TIMER6 , TIMER_INT_FLAG_UP); 
	timer_enable(TIMER6);
	timer_interrupt_enable(TIMER6 , TIMER_INT_UP);
}

inline void
vMBPortTimersDisable()
{
	//关闭定时器和更新中断，并清楚可能残留的中断标志
	timer_interrupt_disable(TIMER6 , TIMER_INT_UP);
	timer_interrupt_flag_clear(TIMER6 , TIMER_INT_FLAG_UP);
	timer_disable(TIMER6);
}





