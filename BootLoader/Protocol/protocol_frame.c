#include "uart_protocol.h"

int Protocol_Frame(uint8_t *buf, uint16_t len, ProtocolFrame *frame)
{
    uint16_t head;
    uint16_t tail;
    uint16_t r_crc;
    uint16_t cal_crc;
    uint8_t payload_len;

    if(buf == 0 || frame == 0 ) //最小帧长度，包含头、尾、CRC等
    {
        return PROTOCOL_ERR_ASCII;
    }
    if(len < PROTOCOL_MIN_FRAME_SIZE) //协议最小长度，包含头、尾、CRC等
    {
        return PROTOCOL_ERR_LEN_SHORT;
    }

    //检查帧头 
    head = ((uint16_t)(buf[0] << 8) )| buf[1];
    if(head != PROTOCOL_HEAD)
    {
        return PROTOCOL_ERR_HEAD;
    }
    //检查长度
    payload_len = buf[7];

    if(payload_len > PROTOCOL_MAX_PAYLOAD)
    {
        return PROTOCOL_ERR_PAYLOAD;
    }

    if(len != (PROTOCOL_MIN_FRAME_SIZE + payload_len))
    {
        return PROTOCOL_ERR_LEN;
    }
    //检查协议版本
    if(buf[8] != PROTOCOL_VERSION)
    {
        return PROTOCOL_ERR_VERSION;
    }
    //检查帧尾
    tail = ((uint16_t)(buf[len - 2] << 8)) | buf[len - 1];
    if(tail != PROTOCOL_TAIL)
    {
        return PROTOCOL_ERR_TAIL;
    }
    //检查crc
    cal_crc = Protocol_CRC16(buf, len - 4); //CRC计算范围不包含最后的CRC和帧尾
    r_crc = ((uint16_t)(buf[len - 4] << 8)) | buf[len - 3];
    if(r_crc != cal_crc)
    {
        return PROTOCOL_ERR_CRC;
    }
    //提取设备id，帧类型，命令字，payload信息等
    frame->id = ((uint16_t)(buf[2] << 8)) | buf[3];
    frame->frame_type = buf[4];
    frame->cmd = ((uint16_t)(buf[5] << 8)) | buf[6];
    frame->len = payload_len;
    frame->version = buf[8];
    if(payload_len > 0 )
    {
        memcpy(frame->payload, &buf[9], payload_len);
    }
    return PROTOCOL_OK;
}

//组帧发送函数
void Protocol_SendFrame(uint16_t id, uint8_t frame_type, uint16_t cmd, uint8_t *payload, uint8_t payload_len)
{
    uint8_t tx_bin[PROTOCOL_MAX_FRAME_SIZE];
    uint8_t tx_hex[PROTOCOL_MAX_FRAME_SIZE * 2];
    uint16_t bin_len = 0;
    uint16_t hex_len = 0;

    //写入帧头
    tx_bin[bin_len++] = (PROTOCOL_HEAD >> 8) & 0xFF;
    tx_bin[bin_len++] = PROTOCOL_HEAD & 0xFF;
    //写入设备id
    tx_bin[bin_len++] = (id >> 8) & 0xFF;
    tx_bin[bin_len++] = id & 0xFF;
    //写入帧类型
    tx_bin[bin_len++] = frame_type;
    //写入命令字
    tx_bin[bin_len++] = (cmd >> 8) & 0xFF;
    tx_bin[bin_len++] = cmd & 0xFF;
    //写入payload长度
    tx_bin[bin_len++] = payload_len;
    //写入协议版本
    tx_bin[bin_len++] = PROTOCOL_VERSION;
    //写入payload
    if(payload_len > 0 && payload != 0)
    {
        memcpy(&tx_bin[bin_len], payload, payload_len);
        bin_len += payload_len;
    }
    //写入crc
    uint16_t crc = Protocol_CRC16(tx_bin, bin_len);
    tx_bin[bin_len++] = (crc >> 8) & 0xFF;
    tx_bin[bin_len++] = crc & 0xFF;

    //写入帧尾
    tx_bin[bin_len++] = (PROTOCOL_TAIL >> 8) & 0xFF;
    tx_bin[bin_len++] = PROTOCOL_TAIL & 0xFF;

    //转成ASCII
    Protocol_Bin2HexStr(tx_bin, bin_len, tx_hex, &hex_len);

    //485—send发送
    USART485_Send(tx_hex, hex_len);
}
//发送OK应答
void Protocol_SendOK(uint16_t id, uint16_t cmd)
{
    uint8_t ok = PROTOCOL_OK_VALUE;
    Protocol_SendFrame(id, FRAME_TYPE_REPLY, cmd, &ok, 1);
}

//发送错误应答
void Protocol_SendError(uint16_t id)
{
    Protocol_SendFrame(id, FRAME_TYPE_ERROR, CMD_ERROR_RPL, 0, 0);
}

void Protocol_SendErrorCmd(uint16_t id, uint16_t cmd)
{
    Protocol_SendFrame(id, FRAME_TYPE_ERROR, cmd, 0, 0);
}

//发送心跳帧
void Protocol_SendHeartBeat(uint16_t id)
{
    Protocol_SendFrame(id, FRAME_TYPE_BEAT, CMD_HEART_BEAT, 0, 0);
}
