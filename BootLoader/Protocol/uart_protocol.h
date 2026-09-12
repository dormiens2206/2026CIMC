#ifndef __PROTOCOL_UART_H
#define __PROTOCOL_UART_H

#include "HeaderFiles.h"

#define PROTOCOL_HEAD         0xA5B6      //帧头固定值
#define PROTOCOL_TAIL           0xB6A5       //帧尾固定值
#define PROTOCOL_VERSION          0x02       //协议版本号

//每个字段的字节长
#define PROTOCOL_HEAD_SIZE    2
#define PROTOCOL_ID_SIZE      2
#define PROTOCOL_TYPE_SIZE    1
#define PROTOCOL_CMD_SIZE     2
#define PROTOCOL_LEN_SIZE     1
#define PROTOCOL_VER_SIZE     1
#define PROTOCOL_CRC_SIZE     2
#define PROTOCOL_TAIL_SIZE    2

//最小帧结构
#define PROTOCOL_MIN_FRAME_SIZE    13
//最大data长度
#define PROTOCOL_MAX_PAYLOAD       255
#define PROTOCOL_MAX_FRAME_SIZE    (PROTOCOL_MIN_FRAME_SIZE + PROTOCOL_MAX_PAYLOAD)

//帧类型定义
#define FRAME_TYPE_CMD         0x01    //命令下发帧
#define FRAME_TYPE_REPLY       0x02    //应答帧
#define FRAME_TYPE_BEAT        0x05    //心跳帧
#define FRAME_TYPE_ERROR       0xFF    //错误帧

//命令字定义

//系统管理类0x01
#define CMD_RESET       0x0101  //重启设备      
#define CMD_VISION_Q    0x0104  //查询版本
#define CMD_TIME_SET    0x0105  //设置时间
#define CMD_TIME_Q      0x0106  //查询时间
#define CMD_ID_Set      0x01A1  //设置设备ID
#define CMD_BAUD_Set    0x01A2  //设置波特率
#define CMD_ID_Q        0x0111  //查询设备ID
#define CMD_BAUD_Q      0x0112  //查询波特率

//数据类0x02
#define CMD_CH0_Q    0x0201  //查询CH0
#define CMD_CH1_Q    0x0202  //查询CH1(DAC回读)
#define CMD_CH2_Q    0x0221  //查询特定通道（例如PT100）
#define CMD_CH0_SER  0x0241  //设置CH0变比
#define CMD_CH1_SET  0x0242  //设置CH1变比
#define CMD_INTERVAL_SET  0x0261  //设置上报间隔

//控制类0x03
#define CMD_DAC_SET   0x0301  //设置DAC输出
#define CMD_TIME_START 0x0302  //启动定时上报
#define CMD_TIME_STOP  0x0303  //停止定时上报
#define CMD_ENTER_SLEEP  0x03AA  //进入睡眠

//参数配置类0x04
#define CMD_THRESHOLD_R  0x0400  //读取阈值
#define CMD_TH_CH0_R     0x0401  //读取CH0阈值
#define CMD_TH_CH1_R     0x0402  //读取CH1阈值
#define CMD_TH_CH2_R     0x0403  //读取CH2阈值
#define CMD_TH_CH0_W     0x0411  //写入CH0阈值
#define CMD_TH_CH1_W     0x0412  //写入CH1阈值
#define CMD_TH_CH2_W     0x0413  //写入CH2阈值

//系统升级类0x05
#define CMD_UPDATE_Q    0x0501  //升级请求
#define CMD_TRANS_DATA    0x0502  //固件升级数据包传输
#define CMD_UPDATE_EX    0x0503  //执行升级

//告警与日志类0x06
#define CMD_ALARM_SUB   0x0601  //询问是否上报告警
#define CMD_ALARM_Q       0x0602  //查询告警记录
#define CMD_ALARM_Clear   0x0603  //清除告警记录

//特殊命令字
#define CMD_HEART_BEAT    0x8888     //心跳包命令字
#define CMD_INFORM        0xFFFF     //上电通知
#define CMD_FIND_DEV      0xFFFF     //寻找设备命令
#define CMD_ERROR_RPL   0xEEEE      //错误应答


//协议错误码
#define PROTOCOL_OK                 0
#define PROTOCOL_ERR_ASCII         -1
#define PROTOCOL_ERR_LEN_SHORT     -2
#define PROTOCOL_ERR_HEAD          -3
#define PROTOCOL_ERR_LEN           -4
#define PROTOCOL_ERR_VERSION       -5
#define PROTOCOL_ERR_TAIL          -6
#define PROTOCOL_ERR_CRC           -7
#define PROTOCOL_ERR_PAYLOAD       -8

#define PROTOCOL_OK_VALUE          0xFF     

//有效报文格式（除包头包尾crc校验字）
typedef struct
{
    uint16_t id;         //设备ID
    uint8_t  frame_type;        //帧类型
    uint16_t cmd;           //命令字
    uint8_t  len;
    uint8_t  version;       //协议版本，后续bootloader升级
    uint8_t  payload[PROTOCOL_MAX_PAYLOAD]; //传输的内容，长度由len字段指定
} ProtocolFrame;



/*设备ID,广播命令字*/

#define DEVICE_ID_DEFAULT      0x0001U
#define DEVICE_ID_BROADCAST    0xFFFFU

void Protocol_Task(void);
uint8_t Bootloader_IsFirmwareReady(void);
uint8_t Bootloader_IsUpgradeDone(void);


int Protocol_HexStr2Bin(uint8_t *hexstr, uint16_t hexstr_len,uint8_t *bin, uint16_t *bin_len);
uint8_t Protocol_Char2Value(uint8_t ch);
void Protocol_Bin2HexStr(uint8_t *bin, uint16_t bin_len, uint8_t *hexstr, uint16_t *hexstr_len);
uint16_t Protocol_CRC16(uint8_t *data, uint16_t len);

uint16_t Protocol_Swap16(uint16_t val);
uint32_t Protocol_Swap32(uint32_t val);
void Protocol_Float2BigEnd(float *val, uint8_t *bytes);
float Protocol_BigEndian2Float(uint8_t *bytes);

int Protocol_Frame(uint8_t *buf, uint16_t len, ProtocolFrame *frame);
void Protocol_SendFrame(uint16_t id, uint8_t frame_type, uint16_t cmd, uint8_t *payload, uint8_t payload_len);
void Protocol_SendOK(uint16_t id, uint16_t cmd);
void Protocol_SendError(uint16_t id);
void Protocol_SendHeartBeat(uint16_t id);

void Protocol_SendErrorCmd(uint16_t id, uint16_t cmd);

#endif

