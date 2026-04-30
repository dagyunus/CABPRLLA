#include <msp430.h>
#include "hal_gpio.h"
#include "hal_uart.h"
#include "hal_adc.h"
#include "modbus.h"

void main(void) {
    WDTCTL = WDTPW | WDTHOLD; // Watchdog kapalı
    
    HAL_GPIO_InitClock();
    HAL_GPIO_InitPins();
    HAL_UART_Init();
    HAL_ADC_Init();
    MODBUS_Init();
    
    _BIS_SR(GIE); // Global kesmeleri aç
    
    while(1) {
        // Sadece Modbus Haberleşme (Risk içermeyen standart döngü)
        if (SCIB.RecData.Counter > 0) {
            if (++SCIB.RecData.Idle > IDLE_TIMEOUT) {
                MODBUS_Process();
                SCIB.RecData.Counter = 0;
                SCIB.RecData.Idle = 0;
            }
        }
    }
}
