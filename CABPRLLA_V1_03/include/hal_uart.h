#ifndef HAL_UART_H_
#define HAL_UART_H_

#include <msp430.h>
#include <stdint.h>

#define BUFFER_SIZE 64U

/* UART ayarları makrolaştırıldı. Clock tanımı mevcut haliyle korunur. */
#define HAL_UART_BR0_VALUE       10U
#define HAL_UART_BR1_VALUE       0U
#define HAL_UART_MCTL_VALUE      UCBRS1

#define HAL_UART_TX_PRE_DELAY    1000U
#define HAL_UART_TX_POST_DELAY   2500U

typedef uint8_t (*HAL_UART_TxProvider_t)(uint8_t *out_byte);

typedef struct
{
    struct
    {
        uint8_t  Data[BUFFER_SIZE];
        uint16_t Length;
        uint16_t Counter;
    } SentData;

    struct
    {
        uint8_t  Data[BUFFER_SIZE];
        volatile uint16_t Counter;
        volatile uint16_t Idle;
        volatile uint8_t  Overflow;
    } RecData;

} SCI_Handle_t;

extern SCI_Handle_t SCIB;

void HAL_UART_Init(void);
void HAL_UART_SendPacket(void);
void HAL_UART_StartStream(HAL_UART_TxProvider_t provider);
void HAL_UART_ClearRx(void);
uint8_t HAL_UART_TakeRxFrame(uint8_t *dst, uint16_t *len, uint16_t max_len);
uint8_t HAL_UART_TickRxIdle(uint8_t *dst, uint16_t *len, uint16_t max_len, uint16_t idle_timeout);

#endif
