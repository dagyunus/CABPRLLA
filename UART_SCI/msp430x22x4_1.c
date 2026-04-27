#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

#define CRC16_POLY             0x1021

#define RS485_TX()             (P4OUT &= ~BIT7)
#define RS485_RX()             (P4OUT |= BIT7)

#define SCI_MODULE_ADDR        0x01
#define SCI_OPCODE_READ        0x03

#define SCI_MAX_FRAME_BYTES    64
#define SCI_IDLE_TIMEOUT_COUNT 6000

volatile uint8_t sci_rx_buf[SCI_MAX_FRAME_BYTES];
volatile uint16_t sci_rx_count = 0;

void clock_init_manual(void);
void gpio_init(void);
void uart_init_115200(void);

bool uart_byte_available(void);
uint8_t uart_read_byte(void);
void uart_send_byte(uint8_t data);
void uart_wait_tx_complete(void);

void SCI_ProcessPacket(uint8_t *buf, uint16_t len);
void SCI_SendReadResponse(uint16_t address, uint16_t read_len);

uint16_t SCI_CRC_Calc(uint8_t *buf, uint16_t len);
uint16_t SCI_CRC_Update(uint16_t crc, uint8_t data);
void SCI_SendByteWithCRC(uint8_t data, uint16_t *crc);

int main(void)
{
    uint16_t idle_counter = 0;

    WDTCTL = WDTPW | WDTHOLD;

    clock_init_manual();
    gpio_init();
    uart_init_115200();

    while (1)
    {
        if (uart_byte_available())
        {
            uint8_t rx = uart_read_byte();

            if (sci_rx_count < SCI_MAX_FRAME_BYTES)
            {
                sci_rx_buf[sci_rx_count++] = rx;
            }
            else
            {
                sci_rx_count = 0;
            }

            idle_counter = 0;
        }
        else
        {
            if (sci_rx_count > 0)
            {
                idle_counter++;

                if (idle_counter > SCI_IDLE_TIMEOUT_COUNT)
                {
                    SCI_ProcessPacket((uint8_t *)sci_rx_buf, sci_rx_count);

                    sci_rx_count = 0;
                    idle_counter = 0;
                }
            }
        }
    }
}

void clock_init_manual(void)
{
    BCSCTL1 = XT2OFF | RSEL2 | RSEL1 | RSEL0;
    DCOCTL  = DCO1 | DCO0;
    BCSCTL2 = 0x00;
}

void gpio_init(void)
{
    P3SEL |= BIT4 | BIT5;

    P4SEL &= ~BIT7;
    P4DIR |= BIT7;

    RS485_RX();
}

void uart_init_115200(void)
{
    UCA0CTL1 |= UCSWRST;

    UCA0CTL1 |= UCSSEL_2;

    UCA0BR0 = 10;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS1;

    UCA0CTL1 &= ~UCSWRST;
}

bool uart_byte_available(void)
{
    return (IFG2 & UCA0RXIFG) ? true : false;
}

uint8_t uart_read_byte(void)
{
    return UCA0RXBUF;
}

void uart_send_byte(uint8_t data)
{
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}

void uart_wait_tx_complete(void)
{
    while (UCA0STAT & UCBUSY);
}

void SCI_ProcessPacket(uint8_t *buf, uint16_t len)
{
    uint16_t address;
    uint16_t read_len;
    uint16_t crc_rx;
    uint16_t crc_calc;

    if (len != 8)
        return;

    if (buf[0] != SCI_MODULE_ADDR)
        return;

    if (buf[1] != SCI_OPCODE_READ)
        return;

    crc_rx = ((uint16_t)buf[6] << 8) | buf[7];
    crc_calc = SCI_CRC_Calc(buf, 6);

    if (crc_rx != crc_calc)
        return;

    address  = ((uint16_t)buf[2] << 8) | buf[3];
    read_len = ((uint16_t)buf[4] << 8) | buf[5];

    if (read_len == 0)
        return;

    if (read_len > 10)
        read_len = 10;

    SCI_SendReadResponse(address, read_len);
}

void SCI_SendReadResponse(uint16_t address, uint16_t read_len)
{
    uint16_t crc = 0xFFFF;
    uint16_t byte_count;
    uint16_t i;
    uint8_t data;

    byte_count = read_len * 4;

    RS485_TX();
    __delay_cycles(100);

    /*
     * Üst sistem SCI_SentData formatı:
     *
     * Data[0] = ModuleNo + (OpCode << 8)
     * Hatta: 01 03
     *
     * Data[1] = ByteCount << 8
     * Hatta: 00 ByteCount
     */
    SCI_SendByteWithCRC(SCI_MODULE_ADDR, &crc);
    SCI_SendByteWithCRC(SCI_OPCODE_READ, &crc);

    SCI_SendByteWithCRC(0x00, &crc);
    SCI_SendByteWithCRC((uint8_t)(byte_count & 0xFF), &crc);

    /*
     * Test data.
     * Address=0, ReadLen=1 için:
     * 00 01 02 03
     */
    for (i = 0; i < byte_count; i++)
    {
        data = (uint8_t)(((address * 4) + i) & 0xFF);
        SCI_SendByteWithCRC(data, &crc);
    }

    uart_send_byte((uint8_t)((crc >> 8) & 0xFF));
    uart_send_byte((uint8_t)( crc       & 0xFF));

    uart_wait_tx_complete();

    __delay_cycles(100);

    RS485_RX();
}

void SCI_SendByteWithCRC(uint8_t data, uint16_t *crc)
{
    uart_send_byte(data);
    *crc = SCI_CRC_Update(*crc, data);
}

uint16_t SCI_CRC_Calc(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        crc = SCI_CRC_Update(crc, buf[i]);
    }

    return crc;
}

uint16_t SCI_CRC_Update(uint16_t crc, uint8_t data)
{
    uint16_t msg;
    uint8_t i;

    msg = ((uint16_t)data << 8);

    for (i = 0; i < 8; i++)
    {
        if ((msg ^ crc) & 0x8000)
        {
            crc = (crc << 1) ^ CRC16_POLY;
        }
        else
        {
            crc <<= 1;
        }

        msg <<= 1;
    }

    return crc;
}