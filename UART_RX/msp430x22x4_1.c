#include <msp430.h>

#define RS485_CTRL_DIR    P4DIR
#define RS485_CTRL_OUT    P4OUT
#define RS485_CTRL_PIN    BIT7

#define RS485_RX_MODE()   (RS485_CTRL_OUT |=  RS485_CTRL_PIN)
#define RS485_TX_MODE()   (RS485_CTRL_OUT &= ~RS485_CTRL_PIN)

#define RX_BUF_SIZE       64

volatile unsigned char rx_buf[RX_BUF_SIZE];
volatile unsigned char rx_index = 0;
volatile unsigned char rx_packet_ready = 0;

void Clock_Init(void)
{
    if (CALBC1_1MHZ != 0xFF)
    {
        DCOCTL = 0;
        BCSCTL1 = CALBC1_1MHZ;
        DCOCTL = CALDCO_1MHZ;
    }
}

void GPIO_Init(void)
{
    RS485_CTRL_DIR |= RS485_CTRL_PIN;
    RS485_RX_MODE();
}

void UART_Init(void)
{
    P3SEL |= BIT4 | BIT5;

    UCA0CTL1 |= UCSWRST;
    UCA0CTL1 |= UCSSEL_2;

    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS0;

    UCA0CTL1 &= ~UCSWRST;

    IFG2 &= ~UCA0RXIFG;
    IE2  |= UCA0RXIE;
}

void UART_SendByte(unsigned char data)
{
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}

void RS485_SendBuffer(volatile unsigned char *buf, unsigned char len)
{
    unsigned char i;

    RS485_TX_MODE();

    for (i = 0; i < len; i++)
    {
        UART_SendByte(buf[i]);
    }

    while (UCA0STAT & UCBUSY);

    RS485_RX_MODE();
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    unsigned char data;

    if (IFG2 & UCA0RXIFG)
    {
        data = UCA0RXBUF;

        if (rx_index < RX_BUF_SIZE)
        {
            rx_buf[rx_index++] = data;
        }

        // Test için tek byte geldiyse hazır say
        rx_packet_ready = 1;
    }
}

int main(void)
{
    unsigned char len;

    WDTCTL = WDTPW | WDTHOLD;

    Clock_Init();
    GPIO_Init();
    UART_Init();

    __bis_SR_register(GIE);

    while (1)
    {
        if (rx_packet_ready)
        {
            __delay_cycles(3000);   // 9600 baud için kısa bekleme

            IE2 &= ~UCA0RXIE;

            len = rx_index;
            rx_index = 0;
            rx_packet_ready = 0;

            IE2 |= UCA0RXIE;

            if (len > 0)
            {
                RS485_SendBuffer(rx_buf, len);
            }
        }
    }
}