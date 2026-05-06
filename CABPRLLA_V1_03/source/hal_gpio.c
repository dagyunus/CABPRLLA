#include <msp430.h>
#include "hal_gpio.h"

void HAL_GPIO_InitClock(void)
{
    BCSCTL1 = RSEL2 | RSEL1 | RSEL0;
    DCOCTL = DCO1 | DCO0;
}

void HAL_GPIO_InitPins(void)
{
    /* Port 1: P1.0-P1.4 dijital giriş ve pull-up aktif. */
    P1DIR &= ~DIGITAL_INPUT_MASK;
    P1REN |=  DIGITAL_INPUT_MASK;
    P1OUT |=  DIGITAL_INPUT_MASK;
    P1IE  = 0U;

    /*
     * Port 2:
     * P2.0 / A0 = TEMP_MCU analog girişi.
     * Pull-up/pull-down kapalı bırakılır.
     */
    P2DIR = 0x00U;
    P2REN &= ~ADC_INPUT_TEMP;
    P2OUT &= ~ADC_INPUT_TEMP;
    P2SEL = 0x00U;

    /* Port 3: UART pinleri. */
    P3SEL |= BIT4 | BIT5;

    /* Port 4: P4.0-P4.4 röleler, P4.7 RS485 yön kontrol. */
    P4DIR |= RELAY_MASK | RS485_DIR_MASK;
    P4OUT &= ~RELAY_MASK;
    RS485_RX();
}

void HAL_GPIO_RelaySet(uint32_t val)
{
    P4OUT = (P4OUT & (uint8_t)(~RELAY_MASK)) | ((uint8_t)val & RELAY_MASK);
}
