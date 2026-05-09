/* hal_adc.h */
#ifndef HAL_ADC_H_
#define HAL_ADC_H_

#include <msp430.h>
#include <stdint.h>

#define HAL_ADC_ERROR_VALUE 0xFFFFU

void HAL_ADC_Init(void);
uint16_t HAL_ADC_Read(void);

#endif
