#ifndef MODBUS_REGISTER_MAP_H
#define MODBUS_REGISTER_MAP_H

#define REG_INPUT_SIZE     16U
#define REG_HOLD_SIZE      64U
#define REG_COILS_SIZE     8U
#define REG_DISC_SIZE      8U

#define REG_INPUT_USED_COUNT   12U
#define REG_HOLD_USED_COUNT    60U
#define REG_COILS_USED_COUNT   2U
#define REG_DISC_USED_COUNT    4U

/*
 * Input Register 寄存器地址映射
 * 功能码：04
 * 只读
 * 
 * 0x0000 ~ 0x0005:实时时钟（年月日时分秒）
 * 0x0006:CH0 通道数据
 * 0x0008:CH1 通道数据
 * 0x00010:CH2 通道数据
 */
#define MODBUS_INPUT_START     0x0000U
#define MODBUS_INPUT_MONTH     0x0001U
#define MODBUS_INPUT_DAY       0x0002U
#define MODBUS_INPUT_HOUR      0x0003U
#define MODBUS_INPUT_MINUTE    0x0004U
#define MODBUS_INPUT_SECOND    0x0005U
#define MODBUS_INPUT_CH0       0x0006U
#define MODBUS_INPUT_CH1       0x0008U
#define MODBUS_INPUT_CH2       0x000AU

/* Input Register 缓冲区索引 */
#define MB_INPUT_YEAR          0U
#define MB_INPUT_MONTH         1U
#define MB_INPUT_DAY           2U
#define MB_INPUT_HOUR          3U
#define MB_INPUT_MINUTE        4U
#define MB_INPUT_SECOND        5U
#define MB_INPUT_CH0            6U
#define MB_INPUT_CH1            8U
#define MB_INPUT_CH2            10U

/*
 * Holding Register 寄存器地址映射
 * 功能码：03 / 06 / 16
 * 可读可写
 */

#define MODBUS_HOLD_YEAR          0x0000U
#define MODBUS_HOLD_MONTH         0x0001U
#define MODBUS_HOLD_DAY           0x0002U
#define MODBUS_HOLD_HOUR          0x0003U 
#define MODBUS_HOLD_MINUTE        0x0004U
#define MODBUS_HOLD_SECOND        0x0005U

#define MODBUS_HOLD_COMM_ADDR     0x0010U       // Modbus从站地址
#define MODBUS_HOLD_COMM_BAUD     0x0011U       // RS485串口波特率
#define MODBUS_HOLD_COMM_DATABITS  0x0012U      // RS485数据位
#define MODBUS_HOLD_COMM_STOPBITS  0x0013U      // 停止位
#define MODBUS_HOLD_COMM_PARITY    0x0014U      // 校验位

#define MODBUS_HOLD_CH0_RATIO      0x0030U      // CH0 变比
#define MODBUS_HOLD_CH1_RATIO      0x0032U      // CH1 变比
#define MODBUS_HOLD_CH2_RATIO      0x0034U      // CH2 变比
#define MODBUS_HOLD_CH0_LIMIT      0x0036U      // CH0 阈值
#define MODBUS_HOLD_CH1_LIMIT      0x0038U      // CH1 阈值
#define MODBUS_HOLD_CH2_LIMIT      0x003AU      // CH2 阈值

/* Holding Register 缓冲区索引 */
#define MB_HOLD_YEAR               0U
#define MB_HOLD_MONTH              1U
#define MB_HOLD_DAY                2U
#define MB_HOLD_HOUR               3U
#define MB_HOLD_MINUTE             4U
#define MB_HOLD_SECOND             5U

#define MB_HOLD_COMM_ADDR          16U
#define MB_HOLD_COMM_BAUD          17U
#define MB_HOLD_COMM_DATABITS      18U
#define MB_HOLD_COMM_STOPBITS      19U
#define MB_HOLD_COMM_PARITY        20U

#define MB_HOLD_CH0_RATIO          48U
#define MB_HOLD_CH1_RATIO          50U
#define MB_HOLD_CH2_RATIO          52U
#define MB_HOLD_CH0_LIMIT          54U
#define MB_HOLD_CH1_LIMIT          56U
#define MB_HOLD_CH2_LIMIT          58U

/*
 * Coil 寄存器地址映射
 * 功能码：01 / 05 / 15
 */

#define MB_COIL_RUN_INDICATOR      0x0000U      // 运行指示灯
#define MB_COIL_CURRENT_BREAK      0x0001U      // 电流通道断线标志

/*
 * Discrete Input 寄存器地址映射
 * 功能码：02
 * 只读
 */

#define MB_DISC_DEVICE_FAULT          0x0000U       //设备故障标志
#define MB_DISC_CH0_OVER_ALARM          0x0001U       //CH0 通道过阈值报警
#define MB_DISC_CH1_OVER_ALARM          0x0002U       //CH1 通道过阈值报警
#define MB_DISC_CH2_OVER_ALARM          0x0003U       //CH2 通道过阈值报警

#endif
