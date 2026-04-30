#ifndef HAL_GPIO_H_
#define HAL_GPIO_H_
#include <stdint.h>

// --- RÖLE ÇIKIŞLARI (P4) ---
#define RL_COMMON_ALARM    BIT0
#define RL_LOAD_ON_BYPASS  BIT1
#define RL_BATTERY_LOW     BIT2
#define RL_LOAD_ON_INV     BIT3
#define RL_MAINS_OK        BIT4

// --- DİJİTAL GİRİŞLER (P1) ---
#define IN_GEN_OP          BIT0
#define IN_CUSTOM          BIT1
#define IN_RMT_SHTDOWN     BIT2
#define IN_AUX_EXT_BYPASS  BIT3
#define IN_AUX_RMT_OUT_SW  BIT4

#define RS485_TX()          (P4OUT &= ~BIT7)
#define RS485_RX()          (P4OUT |=  BIT7)

void HAL_GPIO_InitClock(void);
void HAL_GPIO_InitPins(void);
void HAL_GPIO_RelaySet(uint32_t val);
#endif

