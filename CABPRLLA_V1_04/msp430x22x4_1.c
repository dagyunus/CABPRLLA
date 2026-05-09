#include <msp430.h>
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_adc.h"
#include "modbus.h"

void main(void)
{
    uint8_t rx_frame[BUFFER_SIZE];
    uint16_t rx_len = 0U;

    WDTCTL = WDT_ARST_1000; // Watchdog aktif, yaklaşık 1000 ms timeout

    HAL_GPIO_InitClock();
    HAL_GPIO_InitPins();
    HAL_UART_Init();
    HAL_ADC_Init();
    MODBUS_Init();

    _BIS_SR(GIE);

    while (1)
    {
        WDTCTL = WDTPW | WDTCNTCL;
        
        if (HAL_UART_TickRxIdle(rx_frame, &rx_len, BUFFER_SIZE, IDLE_TIMEOUT))
        {
            MODBUS_ProcessBuffer(rx_frame, rx_len);
        }
    }
}
