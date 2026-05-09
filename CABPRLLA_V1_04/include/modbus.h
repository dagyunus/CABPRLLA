#ifndef MODBUS_H_
#define MODBUS_H_

#include <stdint.h>
#include "hal_uart.h"
#include "hal_adc.h"
#include "hal_gpio.h"

#define DEVICE_ID               0xF6U
#define MB_OP_READ              0x03U
#define MB_OP_WRITE             0x10U

#define MB_EXCEPTION_ILLEGAL_FUNCTION 0x01U
#define MB_EXCEPTION_ILLEGAL_ADDRESS  0x02U
#define MB_EXCEPTION_ILLEGAL_VALUE    0x03U

#define IDLE_TIMEOUT            1000U
#define MB_RELAY_COMMAND        2U
#define MB_TOTAL_REGISTERS_32   5U
#define MB_TABLE_WORD_COUNT     10U

uint32_t Temp_Value; 

// Röle Durumları (P4 Portu)
typedef union {
    uint32_t All;
    struct {
        uint32_t BattLowV_Trig    : 1; // P4.0
        uint32_t LoadOnInv_Trig   : 1; // P4.1
        uint32_t MainsOk_Trig     : 1; // P4.2
        uint32_t CommonAlarm_Trig : 1; // P4.3
        uint32_t LoadOnByps_Trig  : 1; // P4.4
        uint32_t Reserved         : 27;
    } Bits;
} Relay_Status_t;

// Giriş Durumları. P1.0 ADC için ayrıldığı için P1.1-P1.5 sıkıştırılarak bit0-bit4'e yazılır.
typedef union {
    uint32_t All;
    struct {
        uint32_t GenOp_In        : 1; // P1.0
        uint32_t Custom_In       : 1; // P1.1
        uint32_t RmtShtdown_In   : 1; // P1.2
        uint32_t AuxIn1_Byps     : 1; // P1.3
        uint32_t AuxIn2_RmtOutSw : 1; // P1.4
        uint32_t Reserved        : 27;
    } Bits;
} Input_Status_t;

typedef struct {
    uint32_t       ID;           // Adres 0
    Relay_Status_t Relay_Status; // Adres 1
    uint32_t       Relay_CMD;    // Adres 2
    Input_Status_t In_Status;    // Adres 3
    uint32_t       Temp_Value;   // Adres 4
} ModBus_Structure_t;

typedef union {
    uint16_t           All[MB_TABLE_WORD_COUNT];
    ModBus_Structure_t Structure;
} ModBus_Table_t;

extern ModBus_Table_t ModBus_Table;

void MODBUS_Init(void);
void MODBUS_Process(void);
void MODBUS_ProcessBuffer(const uint8_t *rx, uint16_t len);
void MODBUS_UpdateTable(void);

#endif
