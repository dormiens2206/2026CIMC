#include "Function.h"
#include "gd30ad3344.h"
#include "spi_port.h"

//作为 APP 总入口
/*
系统初始化
主循环任务调度
调用协议处理
调用自动上报任务
调用 LED/OLED 刷新
调用告警检测
*/  


//转换电压必备的宏
#define POSITIVE_FS 0x7FFF
#define NEGATIVE_FS 0x8000
#define ADC_DATA    32768

float pga = 5.0f; // 满量程换算系数
extern uint16_t AD3344_CONFIG; // 引用官方驱动库里的全局配置变量

//寄存器配置函数
void AD3344_reg_Config(uint8_t InputMUX, uint8_t Channel)
{
    AD3344_CONFIG = AD3344_CONFIG_DEFAULT;

    if(InputMUX == AD3344_DUAL_END)
    {
        switch (Channel)
        {
        case (0): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_1; break;
        case (1): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_3; break;
        case (2): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_1_3; break;
        case (3): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_2_3; break;
        }
    }else if(InputMUX == AD3344_SINGLE_END)
    {
        switch (Channel)
        {
        case (0): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_0; break;
        case (1): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_1; break;
        case (2): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_2; break;
        case (3): AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_3; break;
        }
    }
    
    AD3344_CONFIG &= ~AD3344_REG_CONFIG_DR_MASK;
    AD3344_CONFIG |= AD3344_REG_CONFIG_DR_1000SPS;

    AD3344_CONFIG &= ~AD3344_REG_CONFIG_PGA_MASK;
    AD3344_CONFIG |= AD3344_REG_CONFIG_PGA_4_096V;

    // 量程配置：实际数值需结合 GD30AD3344 手册与外部基准定义确认
 
    
    AD3344_CONFIG |= AD3344_REG_CONFIG_PULL_UP_EN;
    AD3344_CONFIG |= AD3344_REG_CONFIG_NOP_VALID;
    AD3344_CONFIG |= AD3344_REG_CONFIG_MODE_SINGLE;
    
}

//二进制转电压函数
float adcdata_to_volt(uint16_t bin)
{
    int  _val;
    float adcValue;
    
    if(bin == NEGATIVE_FS){
        adcValue = -(pga*bin/ADC_DATA);
    }else{
        _val     =  bin&NEGATIVE_FS ?  (-((~bin+1)&POSITIVE_FS)):bin;
        adcValue = pga*_val/ADC_DATA;
    }
    return adcValue;
}

void System_Init(void)
{
    System_Hardware_Init();
    RTC_Init();

    spi_flash_init();

    /*ad3344芯片初始化*/
    ad3344_spi_init();
    ad3344_process();
    ad3344_ExtRef();
    AD3344_reg_Config(AD3344_SINGLE_END, 0); // 默认单端通道0
    ad3344_init(AD3344_CONFIG);

    //初始化LED状态
    LED_Stat = 0;
    LED_STATE();

    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t *)"Standby", 16);
    OLED_ShowString(0, 16, (uint8_t *)"System Init OK", 16);
    OLED_Refresh();

}

void System_Hardware_Init(void)
{
    systick_config();

    LED_Init();
    KEY_Init();

    OLED_Init();


    USART485_Init();
    //UART_Protocol_Init();

    ADC_Init();
    DAC_Init();

    PMU_LP_Init();
}

void Init_LED_Stat(void)
{
    LED_Stat = 0;
    LED_STATE();
}


void UsrFunction(void)
{
    
}
