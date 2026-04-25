#include <msp430.h>

void clock_init_manual(void);
void gpio_init(void);
void uart_init_9600(void);
void uart_send_byte(unsigned char data);

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    clock_init_manual();
    gpio_init();
    uart_init_9600();

    while (1)
    {
        uart_send_byte(0xF0);     // 0x55 gönder
        __delay_cycles(1000000);  // ~1 saniye (1 MHz varsayım)
    }
}

void clock_init_manual(void)
{
    // Senin kartta kullandığımız yaklaşık 1 MHz ayar
    BCSCTL1 = XT2OFF | RSEL2 | RSEL1 | RSEL0;
    DCOCTL  = DCO1 | DCO0;

    BCSCTL2 = 0x00;  // MCLK = DCO, SMCLK = DCO
}

void gpio_init(void)
{
    // UART pinleri aktif
    P3SEL |= BIT4 | BIT5;   // P3.4 TX, P3.5 RX

    P4SEL &= ~BIT7;
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;         // sürekli LOW
}

void uart_init_9600(void)
{
    UCA0CTL1 |= UCSWRST;

    UCA0CTL1 |= UCSSEL_2;     // SMCLK

    // SMCLK yaklaşık 1.18 MHz ölçüldü
    // 1.18 MHz / 9600 ≈ 123
    UCA0BR0 = 123;
    UCA0BR1 = 0;

    // Fraction küçük, başlangıç için bu yeterli
    UCA0MCTL = UCBRS0;

    UCA0CTL1 &= ~UCSWRST;
}

void uart_send_byte(unsigned char data)
{
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}