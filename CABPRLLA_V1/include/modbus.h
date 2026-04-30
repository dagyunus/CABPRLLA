#ifndef MODBUS_H_
#define MODBUS_H_

#include <stdint.h>
#include "hal_uart.h"
#include "hal_gpio.h"
#include "hal_adc.h"

/* --- MODBUS AYARLARI --- */
#define DEVICE_ID           0x46u
#define MB_OP_READ          0x03u
#define MB_OP_WRITE         0x10u
#define IDLE_TIMEOUT        30000u

/* --- MODBUS KAYIT ADRESLERİ --- */
#define MB_USER_CMD         50u
#define MB_RELAY_COMMAND    52u

/* --- MODBUS TABLO YAPISI --- */
typedef union {
    struct {
        uint32_t ID;              // Addr 0
        uint32_t Status;          // Addr 1
        uint32_t Reserved1[48];   // Addr 2-49
        uint32_t User_CMD;        // Addr 50
        uint32_t Relay_Status;    // Addr 51
        uint32_t Relay_CMD;       // Addr 52
        uint32_t In_Status;       // Addr 53
        uint32_t Reserved2[4];    // Addr 54-57
        uint32_t Temp_Value;      // Addr 58
    } Structure;
    uint16_t All[118];
} ModBus_Table_t;

extern ModBus_Table_t ModBus_Table;

void MODBUS_Init(void);
void MODBUS_Process(void);
void MODBUS_UpdateTable(void);
#endif
