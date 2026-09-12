#include "uart_protocol.h"

//ASCII字符转化成对应十进制数值
uint8_t Protocol_Char2Value(uint8_t ch)
{
    if(ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    else if(ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    else if(ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    else
    {
        return 0xFF; //无效字符返回0xFF
    }
}

//十六进制字符转二进制
int Protocol_HexStr2Bin(uint8_t *hexstr, uint16_t hexstr_len,uint8_t *bin, uint16_t *bin_len)
{
    uint16_t i;
    uint8_t high;
    uint8_t low;

    if(hexstr == 0 || bin == 0 || bin_len == 0 || hexstr_len % 2 != 0)
    {
        return PROTOCOL_ERR_ASCII; 
    }

    //每两个hex字符转换成一个字节
    *bin_len = hexstr_len / 2;

    if(*bin_len > PROTOCOL_MAX_FRAME_SIZE)
    {
        return PROTOCOL_ERR_LEN;
    }

    for(i = 0; i < *bin_len; i++)
    {
        //将高4位和低4位分别转换成数值后合成一个字节
        high = Protocol_Char2Value(hexstr[2 * i]);
        low = Protocol_Char2Value(hexstr[2 * i + 1]);

        //如果遇到无效字符，返回错误
        if(high == 0xFF || low == 0xFF)
        {
            return PROTOCOL_ERR_ASCII;
        }
        //高位左移4位，与低位按位或
        bin[i] = (high << 4) | low;
    }
    return PROTOCOL_OK;
}

//二进制数据转换成十六进制字符串
void Protocol_Bin2HexStr(uint8_t *bin, uint16_t bin_len, uint8_t *hexstr, uint16_t *hexstr_len)
{
    const char hex_chars[] = "0123456789ABCDEF";

    if(hexstr_len == 0)
    {
        return;
    }

    if(bin == 0 || hexstr == 0 || bin_len == 0)
    {
        *hexstr_len = 0;
        return;
    }

    //每个字节转换成两个hex字符
    *hexstr_len = bin_len * 2;

    uint16_t i;
    for(i = 0; i < bin_len; i++)
    {
        //取出高4位和低4位，转换成对应的hex字符
        hexstr[2 * i] = hex_chars[(bin[i] >> 4) & 0x0F]; //高4位
        hexstr[2 * i + 1] = hex_chars[bin[i] & 0x0F]; //低4位
    }
}

uint16_t Protocol_CRC16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;      //初始值
    uint16_t i,j;

    if(data == 0 || len == 0)
    {
        return 0x0000;
    }

    //对每个字节进行处理
    for(i = 0; i<len; i++)
    {
        crc ^= data[i];
        for(j = 0; j < 8; j++)
        {
            if(crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

//16位整数大小端转换
uint16_t Protocol_Swap16(uint16_t val)
{
    // 将低 8 位左移到高位，将高 8 位右移到低位，然后按位或拼合
    return ((val << 8) & 0xFF00) | ((val >> 8) & 0x00FF);
}
//32位整数大小端转换
uint32_t Protocol_Swap32(uint32_t val)
{
    // 将每个4字节分别移动到对称的位置
    return ((val << 24) & 0xFF000000) | 
           ((val << 8) & 0x00FF0000) | 
           ((val >> 8) & 0x0000FF00) | 
           ((val >> 24) & 0x000000FF);
}
//浮点数转换大端字节
void Protocol_Float2BigEnd(float *val, uint8_t *bytes)
{
    union{
        float f;
        uint8_t b[4];
    }data;
    //将float的4个字节进行交换
    data.f = *val;
    
    if(bytes != 0)
    {
        bytes[0] = data.b[3]; //最高位
        bytes[1] = data.b[2];
        bytes[2] = data.b[1];
        bytes[3] = data.b[0]; //最低位
    }
}
//大端字节转浮点数
float Protocol_BigEndian2Float(uint8_t *bytes)
{
    union{
        float f;
        uint8_t b[4];
    }data;

    if(bytes == 0)
    {
        return 0.0f;
    }

    //将大端字节顺序的4个字节复制到联合体中
    data.b[0] = bytes[3]; //最高位
    data.b[1] = bytes[2];
    data.b[2] = bytes[1];
    data.b[3] = bytes[0]; //最低位

    return data.f;
}
