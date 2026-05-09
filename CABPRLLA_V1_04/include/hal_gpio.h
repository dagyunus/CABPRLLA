#ifndef HAL_GPIO_H_
#define HAL_GPIO_H_

#include <msp430.h>
#include <stdint.h>

// --- RÖLE ÇIKIŞLARI (P4) ---
#define RL_BATTERY_LOW       BIT0
#define RL_LOAD_ON_INV       BIT1
#define RL_MAINS_OK          BIT2
#define RL_COMMON_ALARM      BIT3
#define RL_LOAD_ON_BYPASS    BIT4

#define RELAY_MASK           (RL_BATTERY_LOW | RL_LOAD_ON_INV | RL_MAINS_OK | RL_COMMON_ALARM | RL_LOAD_ON_BYPASS)
#define RS485_DIR_MASK       BIT7

// --- ADC GİRİŞİ ---
// Donanımda TEMP_MCU sinyali P2.0 / A0 üzerindedir.
#define ADC_INPUT_TEMP       BIT0

// --- DİJİTAL GİRİŞLER (P1) ---
// P1.0-P1.4 dijital giriş olarak kullanılır.
#define IN_GEN_OP       BIT0
#define IN_LOAD_ON_INV  BIT1
#define IN_MAINS_OK     BIT2
#define IN_COMMON_ALARM BIT3
#define IN_LOAD_ON_BYP  BIT4

#define DIGITAL_INPUT_MASK  (IN_GEN_OP | IN_LOAD_ON_INV | IN_MAINS_OK | IN_COMMON_ALARM | IN_LOAD_ON_BYP)

#define RS485_TX()           (P4OUT &= ~RS485_DIR_MASK)
#define RS485_RX()           (P4OUT |=  RS485_DIR_MASK)

void HAL_GPIO_InitClock(void);
void HAL_GPIO_InitPins(void);
void HAL_GPIO_RelaySet(uint32_t val);

#endif
