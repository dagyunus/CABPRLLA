#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

/* --- Ayarlar --- */
#define DEVICE_ID_BYTE      0x46u   // Cihaz ID: 70
#define SCI_OPCODE_READ     0x03u
#define SCI_OPCODE_WRITE    0x10u
#define SCI_RX_MAX          64u
#define SCI_TX_MAX          64u
#define IDLE_TIMEOUT        30000u
#define CRC_POLY            0x1021u

#define RS485_TX()          (P4OUT &= ~BIT7)
#define RS485_RX()          (P4OUT |=  BIT7)

/* Register Adresleri */
#define MB_ID               0u
#define MB_STATUS           1u
#define MB_USER_CMD         50u
#define MB_RELAY_STATUS     51u
#define MB_RELAY_COMMAND    52u
#define MB_IN_STATUS        53u
#define MB_TEMP_VALUE       58u

/* Float Sabitleri (IEEE 754) */
#define F_70                0x428C0000UL // 70.0f
#define F_1                 0x3F800000UL // 1.0f
#define F_0                 0x00000000UL // 0.0f

volatile uint8_t  rx_buf[SCI_RX_MAX];
volatile uint16_t rx_cnt = 0;
uint8_t tx_buf[SCI_TX_MAX];

uint32_t mb_status;

/* --- Fonksiyonlar --- */
void init_clock(void) { 
    BCSCTL1 = RSEL2 | RSEL1 | RSEL0; 
    DCOCTL = DCO1 | DCO0; 
}

void init_gpio(void) { 
    P3SEL |= BIT4 | BIT5; // UART
    P1DIR &= ~0x1F; P1REN |= 0x1F; P1OUT |= 0x1F; // Girişler
    P4DIR |= (BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT7);
    P4OUT &= ~(BIT0 | BIT1 | BIT2 | BIT3 | BIT4); 
    RS485_RX(); 
}

void init_uart(void) {
    UCA0CTL1 |= UCSWRST; UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 10; UCA0BR1 = 0; UCA0MCTL = UCBRS1;
    UCA0CTL1 &= ~UCSWRST;
}

void init_adc(void) {
    ADC10CTL1 = INCH_0 + ADC10SSEL_3; 
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON; 
    ADC10AE0 |= BIT0; 
}

uint16_t read_adc(void) {
    uint16_t timeout = 2000;
    ADC10CTL0 |= ENC + ADC10SC;
    while ((ADC10CTL1 & ADC10BUSY) && timeout > 0) timeout--;
    uint16_t res = ADC10MEM;
    ADC10CTL0 &= ~ENC;
    return res;
}

bool uart_available(void) { return (IFG2 & UCA0RXIFG) ? true : false; }
uint8_t uart_read(void) { return UCA0RXBUF; }
void uart_send(uint8_t d) { while (!(IFG2 & UCA0TXIFG)); UCA0TXBUF = d; }

uint16_t CRC(uint8_t *buf, uint16_t len) {
    uint16_t crc = 0; uint16_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= ((uint16_t)buf[i] << 8);
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC_POLY;
            else crc <<= 1;
        }
    }
    return crc;
}

void Relays_Set(uint32_t m) { P4OUT = (P4OUT & ~0x1F) | (uint8_t)(m & 0x1F); }
uint32_t Relays_Get(void) { return (uint32_t)(P4OUT & 0x1F); }

void SCI_SendRead(uint16_t addr, uint16_t len) {
    uint16_t total = len * 4;
    uint16_t i, crc;
    tx_buf[0] = DEVICE_ID_BYTE; tx_buf[1] = SCI_OPCODE_READ;
    tx_buf[2] = 0x00; tx_buf[3] = (uint8_t)(total & 0xFF);
    
    for (i = 0; i < len; i++) {
        uint16_t current_addr = addr + i;
        uint32_t v = 0;
        
        if (current_addr == MB_ID) v = F_70;
        else if (current_addr == MB_STATUS) v = mb_status;
        else if (current_addr == MB_RELAY_STATUS) v = Relays_Get();
        else if (current_addr == MB_IN_STATUS) v = (uint32_t)(P1IN & 0x1F);
        else if (current_addr == MB_TEMP_VALUE) v = (uint32_t)read_adc();
        else v = 0;
        
        tx_buf[4 + i*4] = (uint8_t)(v & 0xFF);
        tx_buf[5 + i*4] = (uint8_t)((v >> 8) & 0xFF);
        tx_buf[6 + i*4] = (uint8_t)((v >> 16) & 0xFF);
        tx_buf[7 + i*4] = (uint8_t)((v >> 24) & 0xFF);
    }
    
    crc = CRC(tx_buf, total + 4);
    tx_buf[total + 4] = (uint8_t)(crc >> 8); tx_buf[total + 5] = (uint8_t)(crc & 0xFF);
    RS485_TX(); __delay_cycles(500);
    for (i = 0; i < (total + 6); i++) uart_send(tx_buf[i]);
    while (UCA0STAT & UCBUSY); __delay_cycles(500); RS485_RX();
}

void SCI_SendAck(uint8_t *buf, uint16_t len) {
    uint16_t i, crc;
    for (i = 0; i < len; i++) tx_buf[i] = buf[i];
    crc = CRC(tx_buf, len);
    tx_buf[len] = (uint8_t)(crc >> 8); tx_buf[len+1] = (uint8_t)(crc & 0xFF);
    RS485_TX(); __delay_cycles(500);
    for (i = 0; i < (len + 2); i++) uart_send(tx_buf[i]);
    while (UCA0STAT & UCBUSY); __delay_cycles(500); RS485_RX();
}

void SCI_Process(uint8_t *buf, uint16_t len) {
    uint16_t crc_rx, addr;
    if (len < 4 || buf[0] != DEVICE_ID_BYTE) return;
    crc_rx = ((uint16_t)buf[len-2] << 8) | buf[len-1];
    if (crc_rx != CRC(buf, len-2)) return;
    addr = ((uint16_t)buf[3] << 8) | buf[2];
    
    if (buf[1] == SCI_OPCODE_READ) {
        uint16_t qty = ((uint16_t)buf[5] << 8) | buf[4];
        SCI_SendRead(addr, qty);
    } else if (buf[1] == SCI_OPCODE_WRITE) {
        uint32_t v = ((uint32_t)buf[6]<<24)|((uint32_t)buf[7]<<16)|((uint32_t)buf[8]<<8)|buf[9];
        if (addr == MB_RELAY_COMMAND) Relays_Set(v);
        else if (addr == MB_USER_CMD) {
            if (v >= 1 && v <= 5) P4OUT |= (1 << (v - 1));
            else if (v >= 11 && v <= 15) P4OUT &= ~(1 << (v - 11));
            else if (v == 20) P4OUT &= ~0x1F;
            else if (v == 21) P4OUT |= 0x1F;
        }
        SCI_SendAck(buf, len - 2);
    }
}

int main(void) {
    uint16_t idle = 0;
    WDTCTL = WDTPW | WDTHOLD;
    init_clock(); init_gpio(); init_uart(); init_adc();
    while (1) {
        mb_status = Relays_Get() ? F_1 : F_0;
        if (uart_available()) {
            uint8_t d = uart_read();
            if (rx_cnt == 0 && d != DEVICE_ID_BYTE) continue;
            if (rx_cnt < SCI_RX_MAX) rx_buf[rx_cnt++] = d;
            idle = 0;
        } else if (rx_cnt > 0) {
            if (++idle > IDLE_TIMEOUT) {
                SCI_Process((uint8_t *)rx_buf, rx_cnt);
                rx_cnt = 0; idle = 0;
            }
        }
    }
}
