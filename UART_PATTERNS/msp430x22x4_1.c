#include <msp430.h>

void clock_init_1mhz(void);
void gpio_init(void);
void uart_init_115200(void);
void rs485_send_byte(unsigned char data);

int main(void)
{
    unsigned char patterns[] = {0xFF, 0x00, 0xAA, 0x55};
    unsigned int i = 0;

    WDTCTL = WDTPW | WDTHOLD;

    clock_init_1mhz();
    gpio_init();
    P4SEL &= ~BIT7;
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;
    uart_init_115200();

    while (1)
    {
        rs485_send_byte(patterns[i]);

        // pattern değiştir
        i++;
        if (i >= 4)
            i = 0;
    }
}

void clock_init_1mhz(void)
{
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL  = CALDCO_1MHZ;
}

void gpio_init(void)
{
    // UART pinleri
    P3SEL |= BIT4 | BIT5;
}

void uart_init_115200(void)
{
    UCA0CTL1 |= UCSWRST;
    UCA0CTL1 |= UCSSEL_2;   // SMCLK = 1MHz

    // 1MHz / 115200 ≈ 8.68
    UCA0BR0 = 8;
    UCA0BR1 = 0;

    // modulation (datasheet’e göre)
    UCA0MCTL = UCBRS2 | UCBRS0;   // ~0x05

    UCA0CTL1 &= ~UCSWRST;
}

void rs485_send_byte(unsigned char data)
{
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}