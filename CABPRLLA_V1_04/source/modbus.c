#include <msp430.h>
#include "modbus.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include <stdio.h>

#define TEMP_ERROR_VALUE_x10    (-9990)
#define ADC_TO_TEMP_GAIN_x10    7873UL
#define ADC_TO_TEMP_DIV         1000UL
ModBus_Table_t ModBus_Table;

/*
 * ADC değerini sıcaklık değerine çevirir.
 *
 * Bu formül 10 mV / °C çıkış veren LM35 benzeri analog sıcaklık sensörü
 * varsayımı ile yazılmıştır.
 *
 * Örnek:
 * 0.255 V -> 25.5 °C
 *
 * Eğer TEMP_MCU hattı NTC, TMP36 veya farklı bir sensör ise
 * sadece bu fonksiyon güncellenmelidir.
 */
static int16_t MODBUS_AdcToTemperatureCx10(uint16_t adc_raw)
{
    uint32_t temp_x10;

    if (adc_raw == HAL_ADC_ERROR_VALUE)
        return TEMP_ERROR_VALUE_x10;

    /*
     * Yuvarlamalı integer hesap:
     * temp_x10 = adc_raw * 7873 / 1000
     */
    temp_x10 = (((uint32_t)adc_raw * ADC_TO_TEMP_GAIN_x10) +
                (ADC_TO_TEMP_DIV / 2UL)) / ADC_TO_TEMP_DIV;

    if (temp_x10 > 32767UL)
        temp_x10 = 32767UL;
    

    return (int16_t)temp_x10;
}


void MODBUS_Init(void)
{
    uint16_t i = MB_TABLE_WORD_COUNT;

    while (i > 0U)
    {
        i--;
        ModBus_Table.All[i] = 0U;
    }

    ModBus_Table.Structure.ID = 0x428C0000UL;
}

void MODBUS_UpdateTable(void)
{
    uint16_t adc_raw;
    float temp_x10;

    /*
     * Röle durumları P4.0-P4.4 üzerinden okunur.
     * P4.7 RS485 yön kontrol olduğu için RELAY_MASK ile filtrelenir.
     */
    ModBus_Table.Structure.Relay_Status.All = (uint32_t)(P4OUT & RELAY_MASK);

    /*
     * Dijital girişler P1.0-P1.4 üzerindedir.
     * ADC girişi bu donanımda P2.0 / A0 olduğu için burada shift yapılmaz.
     */
    ModBus_Table.Structure.In_Status.All = (uint32_t)(P1IN & DIGITAL_INPUT_MASK);

    /*
     * TEMP_MCU ADC ölçümü alınır.
     * Ölçüm MSP430 içinde °C float değere çevrilir.
     * Modbus üzerinden 32-bit IEEE-754 float bitleri gönderilir.
     */
    adc_raw = HAL_ADC_Read();
    temp_x10 = MODBUS_AdcToTemperatureCx10(adc_raw);
    ModBus_Table.Structure.Temp_Value = (uint32_t)((int32_t)temp_x10);
}

/*
 * Mevcut CRC algoritması korunmuştur.
 *
 * CRC gönderim sırası:
 * CRC High, CRC Low
 */
static uint16_t get_CRC16(const uint8_t *ptr, uint16_t len)
{
    uint16_t crc = 0U;

    while (len > 0U)
    {
        uint16_t j = 8U;

        crc ^= ((uint16_t)(*ptr) << 8);
        ptr++;
        len--;

        while (j > 0U)
        {
            j--;

            if ((crc & 0x8000U) != 0U)
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

static uint8_t MODBUS_CheckCRC(const uint8_t *rx, uint16_t len)
{
    uint16_t calc_crc;
    uint16_t rx_crc;

    if ((rx == 0) || (len < 4U))
    {
        return 0U;
    }

    calc_crc = get_CRC16(rx, (uint16_t)(len - 2U));
    rx_crc   = ((uint16_t)rx[len - 2U] << 8) | (uint16_t)rx[len - 1U];

    return (calc_crc == rx_crc) ? 1U : 0U;
}

/*
 * Mantıksal register okuma.
 *
 * ModBus_Table.All[] 16-bit word dizisidir.
 * Her mantıksal register 32-bit kabul edilir.
 *
 * addr = 0 -> All[0] low word, All[1] high word
 * addr = 1 -> All[2] low word, All[3] high word
 *
 * Dışarıya gönderirken 32-bit değer big-endian sırada gönderilir:
 * Data[0] = bit31..24
 * Data[1] = bit23..16
 * Data[2] = bit15..8
 * Data[3] = bit7..0
 */
static uint32_t MODBUS_GetRegisterValue(uint16_t addr)
{
    uint16_t word_index;

    if (addr >= MB_TOTAL_REGISTERS_32)
    {
        return 0UL;
    }

    word_index = (uint16_t)(addr * 2U);

    return ((uint32_t)ModBus_Table.All[(uint16_t)(word_index + 1U)] << 16) |
            (uint32_t)ModBus_Table.All[word_index];
}

/*
 * Read response formatı:
 *
 * ID | 03 | Byte Count | Data... | CRC Hi | CRC Lo
 *
 * Her mantıksal register 32-bit olduğu için:
 *
 * Byte Count = qty * 4
 *
 * Örnek:
 * Register değeri 0x0000001F ise:
 *
 * F6 03 04 00 00 00 1F CRC_H CRC_L
 *
 * Sıcaklık değeri 25.5f ise:
 *
 * 25.5f = 0x41CC0000
 *
 * F6 03 04 41 CC 00 00 CRC_H CRC_L
 */
static void MODBUS_SendReadResponse(uint16_t addr, uint16_t qty)
{
    uint16_t crc;
    uint16_t i = 0U;
    uint16_t out_index = 3U;
    uint32_t val;

    SCIB.SentData.Data[0] = DEVICE_ID;
    SCIB.SentData.Data[1] = MB_OP_READ;
    SCIB.SentData.Data[2] = (uint8_t)(qty * 4U);

    while (i < qty)
    {
        val = MODBUS_GetRegisterValue((uint16_t)(addr + i));

        SCIB.SentData.Data[out_index] = (uint8_t)((val >> 24) & 0xFFU);
        out_index++;

        SCIB.SentData.Data[out_index] = (uint8_t)((val >> 16) & 0xFFU);
        out_index++;

        SCIB.SentData.Data[out_index] = (uint8_t)((val >> 8) & 0xFFU);
        out_index++;

        SCIB.SentData.Data[out_index] = (uint8_t)(val & 0xFFU);
        out_index++;

        i++;
    }

    crc = get_CRC16(SCIB.SentData.Data, out_index);

    SCIB.SentData.Data[out_index] = (uint8_t)(crc >> 8);
    out_index++;

    SCIB.SentData.Data[out_index] = (uint8_t)(crc & 0xFFU);
    out_index++;

    SCIB.SentData.Length = out_index;

    HAL_UART_SendPacket();
}

void MODBUS_ProcessBuffer(const uint8_t *rx, uint16_t len)
{
    uint16_t addr;
    uint16_t qty;
    uint16_t k;
    uint32_t val;

    /*
     * Tüm hatalarda sessiz kal.
     *
     * Eksik frame, NULL pointer, yanlış ID, CRC hatası,
     * geçersiz adres, geçersiz qty veya desteklenmeyen function code
     * durumlarında cevap gönderilmez.
     */
    if ((rx == 0) || (len < 8U))
    {
        return;
    }

    if (rx[0] != DEVICE_ID)
    {
        return;
    }

    if (MODBUS_CheckCRC(rx, len) == 0U)
    {
        return;
    }

    addr = ((uint16_t)rx[2] << 8) | (uint16_t)rx[3];
    qty  = ((uint16_t)rx[4] << 8) | (uint16_t)rx[5];

    /*
     * Cevap verilmeden önce tablo güncellenir.
     * Böylece input, röle ve sıcaklık bilgisi güncel okunur.
     */
    MODBUS_UpdateTable();

    if (rx[1] == MB_OP_READ)
    {
        /*
         * Read request formatı:
         *
         * ID | 03 | Addr Hi | Addr Lo | Qty Hi | Qty Lo | CRC Hi | CRC Lo
         *
         * Response:
         *
         * ID | 03 | Byte Count | 32-bit Data... | CRC Hi | CRC Lo
         */

        if (qty == 0U)
        {
            return;
        }

        /*
         * Cevap buffer kontrolü.
         *
         * Sabit alanlar:
         * ID        = 1 byte
         * Function  = 1 byte
         * ByteCount = 1 byte
         * CRC       = 2 byte
         *
         * Toplam sabit = 5 byte
         * Data alanı   = qty * 4 byte
         */
        if (((uint32_t)qty * 4UL) > (uint32_t)(BUFFER_SIZE - 5U))
        {
            return;
        }

        if (((uint32_t)addr + (uint32_t)qty) > (uint32_t)MB_TOTAL_REGISTERS_32)
        {
            return;
        }

        MODBUS_SendReadResponse(addr, qty);
    }
    else if (rx[1] == MB_OP_WRITE)
    {
        /*
         * MB_OP_WRITE 0x10 olarak korunmuştur.
         * Mevcut özel 8 byte write formatı değiştirilmedi.
         *
         * Write request formatı:
         *
         * ID | 10 | Addr Hi | Addr Lo | Data Hi | Data Lo | CRC Hi | CRC Lo
         */

        if (len != 8U)
        {
            return;
        }

        if (addr != MB_RELAY_COMMAND)
        {
            return;
        }

        val = (uint32_t)(((uint16_t)rx[4] << 8) | (uint16_t)rx[5]);

        /*
         * Sadece röle bitleri kabul edilir.
         * P4.0-P4.4 dışındaki bitler tabloya da yazılmaz, çıkışa da uygulanmaz.
         */
        val &= RELAY_MASK;

        ModBus_Table.Structure.Relay_CMD = val;
        HAL_GPIO_RelaySet(val);

        /*
         * Geçerli write komutunda mevcut sistem mantığı korunur:
         * Gelen 8 byte frame aynen cevap olarak geri gönderilir.
         */
        SCIB.SentData.Length = 8U;

        k = 8U;

        while (k > 0U)
        {
            k--;
            SCIB.SentData.Data[k] = rx[k];
        }

        HAL_UART_SendPacket();
    }
    else
    {
        /*
         * Desteklenmeyen function code.
         * Kullanıcı isteği gereği exception response yok, sessiz kal.
         */
        return;
    }
}

void MODBUS_Process(void)
{
    MODBUS_ProcessBuffer(SCIB.RecData.Data, SCIB.RecData.Counter);
}

