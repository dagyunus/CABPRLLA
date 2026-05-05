#include <msp430.h>
#include "modbus.h"

ModBus_Table_t ModBus_Table;

void MODBUS_Init(void)
{
    uint16_t i = 118U;

    while (i--)
    {
        ModBus_Table.All[i] = 0U;
    }

    /*
     * 70.0f = 0x428C0000
     */
    ModBus_Table.Structure.ID = 0x428C0000UL;
}

void MODBUS_UpdateTable(void)
{
    ModBus_Table.Structure.Status =
        (P4OUT & MB_USER_CMD_MASK) ? 0x3F800000UL : 0UL;

    ModBus_Table.Structure.Relay_Status =
        (uint32_t)(P4OUT & MB_USER_CMD_MASK);

    ModBus_Table.Structure.In_Status =
        (uint32_t)(P1IN & MB_USER_CMD_MASK);

    ModBus_Table.Structure.Temp_Value =
        (uint32_t)HAL_ADC_Read();
}

/*
 * CRC-16/BUYPASS
 * Polynomial : 0x8005
 * Init       : 0x0000
 * RefIn      : false
 * RefOut     : false
 * XorOut     : 0x0000
 *
 * Paket sonunda CRC High Byte, Low Byte olarak gönderiliyor.
 */
static uint16_t get_CRC16(uint8_t *ptr, uint16_t len)
{
    uint16_t crc = 0U;

    while (len--)
    {
        uint16_t j = 8U;

        crc ^= ((uint16_t)(*ptr++) << 8);

        while (j--)
        {
            if (crc & 0x8000U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x8005U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

/*
 * Sadece tanımlı adreslere READ cevabı dön.
 * Reserved alanlara cevap dönme.
 *
 * qty = 32-bit register sayısıdır.
 */
static uint8_t MODBUS_IsReadableAddress(uint16_t addr, uint16_t qty)
{
    uint16_t end_addr;

    if (qty == 0U)
        return 0U;
    
    end_addr = (uint16_t)(addr + qty - 1U);
    if ((addr == 0U) && (end_addr <= MB_ADDR_TEMP_VALUE))
        return 1U;

    if ((addr == MB_ADDR_ID)           && (end_addr == MB_ADDR_ID))           return 1U;
    if ((addr == MB_ADDR_STATUS)       && (end_addr == MB_ADDR_STATUS))       return 1U;
    if ((addr == MB_USER_CMD)          && (end_addr == MB_USER_CMD))          return 1U;
    if ((addr == MB_ADDR_RELAY_STATUS) && (end_addr == MB_ADDR_RELAY_STATUS)) return 1U;
    if ((addr == MB_RELAY_COMMAND)     && (end_addr == MB_RELAY_COMMAND))     return 1U;
    if ((addr == MB_ADDR_IN_STATUS)    && (end_addr == MB_ADDR_IN_STATUS))    return 1U;
    if ((addr == MB_ADDR_TEMP_VALUE)   && (end_addr == MB_ADDR_TEMP_VALUE))   return 1U;

    return 0U;
}

/*
 * Sadece bu adreslere WRITE kabul et.
 */
static uint8_t MODBUS_IsWritableAddress(uint16_t addr)
{
    if (addr == MB_USER_CMD)
    {
        return 1U;
    }

    if (addr == MB_RELAY_COMMAND)
    {
        return 1U;
    }

    return 0U;
}

/*
 * MB_USER_CMD için sadece P4.0-P4.4 maskesi kabul edilir.
 *
 * Geçerli:
 * 0x00 - 0x1F
 *
 * Geçersiz:
 * 0x20, 0xFF, vb.
 */
static uint8_t MODBUS_IsValidUserCmd(uint32_t v)
{
    if (v <= (uint32_t)MB_USER_CMD_MASK)
    {
        return 1U;
    }

    return 0U;
}

/*
 * rx[6]..rx[9] alanını big-endian uint32_t olarak oku.
 *
 * 00 00 00 01 -> 0x00000001
 * 00 00 00 FF -> 0x000000FF
 */
static uint32_t MODBUS_GetU32BE(uint8_t *rx)
{
    return (((uint32_t)rx[6] << 24) |
            ((uint32_t)rx[7] << 16) |
            ((uint32_t)rx[8] << 8)  |
            ((uint32_t)rx[9]));
}

void MODBUS_Process(void)
{
    uint8_t  *rx = SCIB.RecData.Data;
    uint16_t len = SCIB.RecData.Counter;

    uint16_t k;
    uint16_t addr;
    uint16_t qty;
    uint16_t loop_cnt;
    uint16_t crc;

    /*
     * Ortak minimum kontrol.
     */
    if ((len < 4U) || (rx[0] != DEVICE_ID))
    {
        return;
    }

    /*
     * CRC kontrol.
     * Son iki byte CRC_H, CRC_L kabul ediliyor.
     */
    if (get_CRC16(rx, (uint16_t)(len - 2U)) !=
        ((((uint16_t)rx[len - 2U]) << 8) | ((uint16_t)rx[len - 1U])))
    {
        return;
    }

    /*
     * Sadece 0x03 READ ve 0x10 WRITE destekleniyor.
     */
    if ((rx[1] != MB_OP_READ) && (rx[1] != MB_OP_WRITE))
    {
        return;
    }

    /*
     * Adres big-endian.
     */
    addr = ((uint16_t)rx[2] << 8) | ((uint16_t)rx[3]);

    /*
     * Her geçerli paket geldiğinde tabloyu tazele.
     */
    MODBUS_UpdateTable();

    if (rx[1] == MB_OP_READ)
    {
        /*
         * READ paket:
         * ID OP ADDR_H ADDR_L QTY_H QTY_L CRC_H CRC_L
         */
        if (len < 8U)
        {
            return;
        }

        qty = ((uint16_t)rx[4] << 8) | ((uint16_t)rx[5]);

        /*
         * Reserved adreslere cevap dönme.
         */
        if (!MODBUS_IsReadableAddress(addr, qty))
        {
            return;
        }

        /*
         * Tablo dışına çıkmayı engelle.
         * Her 32-bit register = 2 adet uint16_t.
         */
        if ((uint32_t)((addr + qty) * 2U) >
            (uint32_t)(sizeof(ModBus_Table.All) / sizeof(ModBus_Table.All[0])))
        {
            return;
        }

        /*
         * Cevap:
         * [ID][03][00][BYTE_COUNT][DATA...][CRC_H][CRC_L]
         */
        SCIB.SentData.Length = (uint16_t)((qty * 4U) + 6U);

        /*
         * Buffer limiti.
         */
        if (SCIB.SentData.Length > (512U + 6U))
        {
            return;
        }

        {
            uint16_t *p16 = (uint16_t*)SCIB.SentData.Data;

            /*
             * Byte olarak:
             * p16[0] = 0x0346 -> 46 03
             * p16[1] = 0x0400 -> 00 04
             */
            p16[0] = (uint16_t)(DEVICE_ID | (MB_OP_READ << 8));
            p16[1] = (uint16_t)((qty * 4U) << 8);

            k = 0U;
            loop_cnt = (uint16_t)(qty * 2U);

            while (loop_cnt--)
            {
                p16[2U + k] = ModBus_Table.All[(addr * 2U) + k];
                k++;
            }
        }

        crc = get_CRC16(SCIB.SentData.Data,
                        (uint16_t)(SCIB.SentData.Length - 2U));

        SCIB.SentData.Data[SCIB.SentData.Length - 2U] = (uint8_t)(crc >> 8);
        SCIB.SentData.Data[SCIB.SentData.Length - 1U] = (uint8_t)(crc & 0xFFU);

        HAL_UART_SendPacket();
    }
    else if (rx[1] == MB_OP_WRITE)
    {
        uint32_t v;

        /*
         * WRITE paket:
         * ID OP ADDR_H ADDR_L QTY_H QTY_L D0 D1 D2 D3 CRC_H CRC_L
         */
        if (len < 12U)
        {
            return;
        }

        qty = ((uint16_t)rx[4] << 8) | ((uint16_t)rx[5]);

        /*
         * Bu uygulamada sadece 1 adet 32-bit register yazımı kabul ediliyor.
         */
        if (qty != 4U)
            return;


        /*
         * Yazılabilir olmayan adrese cevap dönme.
         */
        if (!MODBUS_IsWritableAddress(addr))
        {
            return;
        }

        v = MODBUS_GetU32BE(rx);

        if (addr == MB_RELAY_COMMAND)
        {
            /*
             * Relay command ayrı HAL fonksiyonu ile işleniyor.
             * Buraya gerekirse ayrıca v limit kontrolü eklenebilir.
             */
            ModBus_Table.Structure.Relay_CMD = v;
            HAL_GPIO_RelaySet(v);
        }
        else if (addr == MB_USER_CMD)
        {
            /*
             * KRİTİK KONTROL:
             * 0xFF gibi değerler burada reddedilir.
             * Reddedilirse aşağıdaki echo cevabına inilmez.
             */
            if (!MODBUS_IsValidUserCmd(v))
            {
                return;
            }

            ModBus_Table.Structure.User_CMD = v;

            /*
             * v bit0..bit4 -> P4.0..P4.4
             *
             * 1 = röle ON
             * 0 = röle OFF
             *
             * Diğer P4OUT bitleri korunur.
             */
            P4OUT = (uint8_t)((P4OUT & (uint8_t)(~MB_USER_CMD_MASK)) |
                              ((uint8_t)v & MB_USER_CMD_MASK));
        }
        else
        {
            return;
        }

        /*
         * Buraya sadece GEÇERLİ WRITE komutları ulaşır.
         * Geçerli WRITE için gelen paketi aynen echo dön.
         */
        SCIB.SentData.Length = len;

        k = len;
        while (k--)
        {
            SCIB.SentData.Data[k] = rx[k];
        }

        HAL_UART_SendPacket();
    }
}

