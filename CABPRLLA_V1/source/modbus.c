#include <msp430.h>
#include "modbus.h"

ModBus_Table_t ModBus_Table;

void MODBUS_Init(void) {
    uint16_t i = 118;
    while(i--) ModBus_Table.All[i] = 0;
    ModBus_Table.Structure.ID = 0x428C0000UL; // 70.0f
}

// Sensör verilerini tabloya yazar
void MODBUS_UpdateTable(void) {
    ModBus_Table.Structure.Status = (P4OUT & 0x1F) ? 0x3F800000UL : 0;
    ModBus_Table.Structure.Relay_Status = (uint32_t)(P4OUT & 0x1F);
    ModBus_Table.Structure.In_Status = (uint32_t)(P1IN & 0x1F);
    ModBus_Table.Structure.Temp_Value = (uint32_t)HAL_ADC_Read();
}

static uint16_t get_CRC16(uint8_t *ptr, uint16_t len) {
    uint16_t crc = 0;
    while(len--) {
        uint16_t j = 8;
        crc ^= ((uint16_t)*ptr++ << 8);
        while(j--) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void MODBUS_Process(void) {
    uint8_t *rx = SCIB.RecData.Data;
    uint16_t len = SCIB.RecData.Counter;
    uint16_t k, addr, qty, loop_cnt;
    
    if (len < 4 || rx[0] != DEVICE_ID) return;
    if (get_CRC16(rx, len - 2) != ((uint16_t)rx[len-2] << 8 | rx[len-1])) return;

    addr = ((uint16_t)rx[3] << 8) | rx[2];
    MODBUS_UpdateTable(); // Tabloyu tazele
    
    if (rx[1] == MB_OP_READ) {
        qty = ((uint16_t)rx[5] << 8) | rx[4];
        uint16_t *p16 = (uint16_t*)SCIB.SentData.Data;
        SCIB.SentData.Length = (qty * 4) + 6;
        
        p16[0] = DEVICE_ID | (MB_OP_READ << 8);
        p16[1] = ((qty * 4) << 8);
        
        // --- MASTER STİLİ OKUMA (Sizin paylaştığınız 332-335. satırlar) ---
        k = 0; loop_cnt = qty * 2; // Her register 2 kelime (32-bit)
        while(loop_cnt--) {
            // Veriyi direkt ModBus_Table.All dizisinden kopyala
            p16[2 + k] = ModBus_Table.All[(addr * 2) + k];
            k++;
        }
        
        uint16_t crc = get_CRC16(SCIB.SentData.Data, SCIB.SentData.Length - 2);
        SCIB.SentData.Data[SCIB.SentData.Length-2] = crc >> 8;
        SCIB.SentData.Data[SCIB.SentData.Length-1] = crc & 0xFF;
        HAL_UART_SendPacket();
    }
    else if (rx[1] == MB_OP_WRITE) {
        uint32_t v = ((uint32_t)rx[6]<<24)|((uint32_t)rx[7]<<16)|((uint32_t)rx[8]<<8)|rx[9];
        
        if (addr == MB_RELAY_COMMAND) HAL_GPIO_RelaySet(v);
        else if (addr == MB_USER_CMD) {
            if (v >= 1 && v <= 5) P4OUT |= (1 << (v - 1));
            else if (v >= 11 && v <= 15) P4OUT &= ~(1 << (v - 11));
            else if (v == 20) P4OUT &= ~0x1F;
            else if (v == 21) P4OUT |= 0x1F;
        }
        
        SCIB.SentData.Length = len;
        k = len;
        while(k--) SCIB.SentData.Data[k] = rx[k];
        HAL_UART_SendPacket();
    }
}
