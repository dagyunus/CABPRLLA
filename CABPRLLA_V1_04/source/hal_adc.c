#include <msp430.h>
#include "hal_adc.h"

#define HAL_ADC_TIMEOUT_COUNT 60000U

static volatile uint8_t g_adcDone = 0U;
static volatile uint16_t g_adcValue = 0U;

void HAL_ADC_Init(void)
{
    /* Kanal 0 (P2.0 / A0 = TEMP_MCU), ADC10 dahili saat. */
    ADC10CTL1 = INCH_0 | ADC10SSEL_3;

    /* Vcc/Vss referans, uzun sample süresi, ADC açık, ADC interrupt açık. */
    ADC10CTL0 = SREF_0 | ADC10SHT_3 | ADC10ON | ADC10IE;

    /* A0 kanalını analog giriş olarak işaretle. Bu donanımda pin P2.0'dır. */
    ADC10AE0 |= BIT0;
}

uint16_t HAL_ADC_Read(void)
{
    uint16_t timeout = HAL_ADC_TIMEOUT_COUNT;

    g_adcDone = 0U;
    g_adcValue = 0U;

    ADC10CTL0 &= ~ENC;
    ADC10CTL0 &= ~ADC10IFG;

    ADC10CTL0 |= ENC | ADC10SC;

    while ((g_adcDone == 0U) && (timeout > 0U))
    {
        timeout--;
    }

    ADC10CTL0 &= ~(ENC | ADC10SC);

    if (timeout == 0U)
    {
        return HAL_ADC_ERROR_VALUE;
    }

    return g_adcValue;
}

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
    g_adcValue = ADC10MEM;
    g_adcDone = 1U;
}
