#include "HeaderFiles.h"

#define APP_START_ADDR  0x08011000U

int main(void)
{
    SCB->VTOR = APP_START_ADDR;
    __DSB();
    __ISB();

    System_Init();

    while (1)
    {
        UsrFunction();
        System_Task();
    }
}
