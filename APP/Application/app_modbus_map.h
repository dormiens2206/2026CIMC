#ifndef APP_MODBUS_MAP_H
#define APP_MODBUS_MAP_H

#include <stdint.h>
#include "modbus_register_map.h"


void App_ModbusInit(void);
void App_ModbusMap_Update(void);
void App_ModbusMap_Apply(void);
void App_ModbusReconfigureTask(void);
uint8_t App_ModbusInputAddressValid(uint16_t addr, uint16_t cnt);
uint8_t App_ModbusHoldingAddressValid(uint16_t addr, uint16_t cnt);
uint8_t App_ModbusCoilsAddressValid(uint16_t addr, uint16_t cnt);
uint8_t App_ModbusDiscreteAddressValid(uint16_t addr, uint16_t cnt);
uint8_t App_ModbusHoldingValueValid(uint16_t index, uint16_t value);

uint8_t App_ModbusParityToMB(uint8_t parity);

#endif
