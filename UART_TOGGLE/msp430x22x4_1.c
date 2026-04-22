#include <msp430.h>

void delay_1ms(void);

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    // 1 MHz clock
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL  = CALDCO_1MHZ;

    // GPIO modu
    P3SEL &= ~(BIT4 | BIT5);
    P4SEL &= ~BIT7;

    // P3.4 -> output, toggle
    P3DIR |= BIT4;
    P3OUT &= ~BIT4;

    // P3.5 -> input
    P3DIR &= ~BIT5;
    P3REN |= BIT5;
    P3OUT |= BIT5;

    // P4.7 -> DE/RE, sürekli LOW
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;

    while(1)
    {
        P3OUT ^= BIT4;
        delay_1ms();
    }
}

void delay_1ms(void)
{
    __delay_cycles(1000);
}