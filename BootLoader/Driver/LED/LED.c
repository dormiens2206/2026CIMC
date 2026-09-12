#include "HeaderFiles.h"

uint8_t LED_Stat = 0;

void LED_Init(void)
{
    rcu_periph_clock_enable(LED_RCU);

    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED1_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED1_PIN);
    gpio_bit_reset(LED_PORT, LED1_PIN);

    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED2_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED2_PIN);
    gpio_bit_reset(LED_PORT, LED2_PIN);

    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED3_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED3_PIN);
    gpio_bit_reset(LED_PORT, LED3_PIN);

    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED4_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED4_PIN);
    gpio_bit_reset(LED_PORT, LED4_PIN);
}

void LED_STATE(void)
{
	if(LED_Stat == 1)
    {
        gpio_bit_set(LED_PORT, LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN);   
    }
    else
    {
        gpio_bit_reset(LED_PORT, LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN);
    }
}

/*
void LED_Status_Update(uint8_t mode)
{
    LED1_OFF();
    LED2_OFF();
    LED3_OFF();
    LED4_OFF();

    switch((Switch_Mode)mode) {
        case SWITCH_MODE_STANDBY:
            LED1_ON();
            break;
        case SWITCH_MODE_ADC:
            LED2_ON();
            break;
        case SWITCH_MODE_PT100_MEASURE:
            LED3_ON();
            break;
        case SWITCH_MODE_RS485_STORAGE:
            LED4_ON();
            break;
        default:
            break;
    }
}*/
