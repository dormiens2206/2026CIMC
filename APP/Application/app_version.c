#include <stdint.h>

/* 固件版本号固化 */
const uint8_t g_app_version[4]
    __attribute__((section(".ARM.__at_0x08011200"), used))
    = {0x14, 0x01, 0x00, 0x00}; // 固件版本号：20.01.00.00

