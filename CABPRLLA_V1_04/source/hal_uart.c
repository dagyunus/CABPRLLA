#include <msp430.h>
#include <stdint.h>

#include "hal_uart.h"
#include "hal_gpio.h"
#include "modbus.h"

SCI_Handle_t SCIB;

static volatile uint8_t g_uart_stream_active = 0U;
static volatile uint8_t g_uart_stream_post_delay = 0U;
static HAL_UART_TxProvider_t g_uart_tx_provider = 0;

void HAL_UART_Init(void)
{
    UCA0CTL1 |= UCSWRST | UCSSEL_2;

    UCA0BR0 = HAL_UART_BR0_VALUE;
    UCA0BR1 = HAL_UART_BR1_VALUE;
    UCA0MCTL = HAL_UART_MCTL_VALUE;

    UCA0CTL1 &= ~UCSWRST;

    IE2 |= UCA0RXIE;

    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL_2 | MC_0;
}

void HAL_UART_ClearRx(void)
{
    __disable_interrupt();
    SCIB.RecData.Counter = 0U;
    SCIB.RecData.Idle = 0U;
    SCIB.RecData.Overflow = 0U;
    __enable_interrupt();
}

uint8_t HAL_UART_TakeRxFrame(uint8_t *dst, uint16_t *len, uint16_t max_len)
{
    uint16_t i;
    uint16_t rx_len;

    if ((dst == 0) || (len == 0))
    {
        return 0U;
    }

    __disable_interrupt();

    if (SCIB.RecData.Overflow)
    {
        SCIB.RecData.Counter = 0U;
        SCIB.RecData.Idle = 0U;
        SCIB.RecData.Overflow = 0U;
        __enable_interrupt();
        *len = 0U;
        return 0U;
    }

    rx_len = SCIB.RecData.Counter;
    if (rx_len > max_len)
        rx_len = max_len;
    
    i =rx_len;
    while (i--)
        dst[i] = SCIB.RecData.Data[i];

    SCIB.RecData.Counter = 0U;
    SCIB.RecData.Idle = 0U;

    __enable_interrupt();

    *len = rx_len;
    return (rx_len > 0U) ? 1U : 0U;
}

uint8_t HAL_UART_TickRxIdle(uint8_t *dst, uint16_t *len, uint16_t max_len, uint16_t idle_timeout)
{
    uint16_t i;
    uint16_t rx_len;

    if ((dst == 0) || (len == 0))
    {
        return 0U;
    }

    *len = 0U;

    __disable_interrupt();

    if (SCIB.RecData.Overflow)
    {
        SCIB.RecData.Counter = 0U;
        SCIB.RecData.Idle = 0U;
        SCIB.RecData.Overflow = 0U;
        __enable_interrupt();
        return 0U;
    }

    if (SCIB.RecData.Counter == 0U)
    {
        SCIB.RecData.Idle = 0U;
        __enable_interrupt();
        return 0U;
    }

    SCIB.RecData.Idle++;

    if (SCIB.RecData.Idle <= idle_timeout)
    {
        __enable_interrupt();
        return 0U;
    }

    rx_len = SCIB.RecData.Counter;
    if (rx_len > max_len)
    {
        rx_len = max_len;
    }
    i =rx_len;
    while (i--)
        dst[i] = SCIB.RecData.Data[i];

    SCIB.RecData.Counter = 0U;
    SCIB.RecData.Idle = 0U;

    __enable_interrupt();

    *len = rx_len;
    return (rx_len > 0U) ? 1U : 0U;
}

void HAL_UART_SendPacket(void)
{
    if ((SCIB.SentData.Length == 0U) || (SCIB.SentData.Length > BUFFER_SIZE))
    {
        return;
    }

    SCIB.SentData.Counter = 0U;

    RS485_TX();

    TA0CCR0 = HAL_UART_TX_PRE_DELAY;
    TA0CTL = TASSEL_2 | TACLR | MC_1;

    __bis_SR_register(LPM0_bits | GIE);

    UCA0TXBUF = SCIB.SentData.Data[SCIB.SentData.Counter++];

    IE2 |= UCA0TXIE;
}

void HAL_UART_StartStream(HAL_UART_TxProvider_t provider)
{
    uint8_t first_byte;

    if (provider == 0)
    {
        return;
    }

    IE2 &= ~UCA0TXIE;

    TA0CTL = MC_0;
    TA0CCTL0 &= ~CCIFG;

    g_uart_tx_provider = provider;
    g_uart_stream_active = 1U;
    g_uart_stream_post_delay = 0U;

    RS485_TX();

    TA0CCR0 = HAL_UART_TX_PRE_DELAY;
    TA0CTL = TASSEL_2 | TACLR | MC_1;

    __bis_SR_register(LPM0_bits | GIE);

    if (g_uart_tx_provider(&first_byte))
    {
        UCA0TXBUF = first_byte;
        IE2 |= UCA0TXIE;
    }
    else
    {
        g_uart_stream_active = 0U;
        g_uart_tx_provider = 0;
        RS485_RX();
    }
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    uint8_t d = UCA0RXBUF;

    if ((SCIB.RecData.Counter == 0U) && (d != DEVICE_ID))
    {
        return;
    }

    if (SCIB.RecData.Counter < BUFFER_SIZE)
    {
        SCIB.RecData.Data[SCIB.RecData.Counter++] = d;
        SCIB.RecData.Idle = 0U;
    }
    else
    {
        SCIB.RecData.Overflow = 1U;
    }
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
    uint8_t next_byte;

    if (g_uart_stream_active)
    {
        if ((g_uart_tx_provider != 0) && g_uart_tx_provider(&next_byte))
        {
            UCA0TXBUF = next_byte;
        }
        else
        {
            IE2 &= ~UCA0TXIE;
            g_uart_stream_post_delay = 1U;
            TA0CCR0 = HAL_UART_TX_POST_DELAY;
            TA0CTL = TASSEL_2 | TACLR | MC_1;
        }
    }
    else
    {
        if (SCIB.SentData.Counter < SCIB.SentData.Length)
        {
            UCA0TXBUF = SCIB.SentData.Data[SCIB.SentData.Counter++];
        }
        else
        {
            IE2 &= ~UCA0TXIE;
            TA0CCR0 = HAL_UART_TX_POST_DELAY;
            TA0CTL = TASSEL_2 | TACLR | MC_1;
        }
    }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A_ISR(void)
{
    TA0CTL = MC_0;
    TA0CCTL0 &= ~CCIFG;

    if (g_uart_stream_post_delay)
    {
        RS485_RX();
        g_uart_stream_active = 0U;
        g_uart_stream_post_delay = 0U;
        g_uart_tx_provider = 0;
    }
    else
    {
        if (!g_uart_stream_active)
        {
            if (SCIB.SentData.Counter >= SCIB.SentData.Length)
            {
                RS485_RX();
            }
        }
    }

    __bic_SR_register_on_exit(LPM0_bits);
}
