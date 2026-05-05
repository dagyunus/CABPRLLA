#ifndef HAL_UART_H_
#define HAL_UART_H_
#include <stdint.h>

#define BUFFER_SIZE 64

typedef struct {
    struct { 
        uint8_t Data[BUFFER_SIZE]; 
        uint16_t Length; 
        uint16_t Counter; 
    } SentData;
    struct { 
        uint8_t  Data[BUFFER_SIZE]; 
        volatile uint16_t Counter; 
        volatile uint16_t Idle; 
    } RecData;
} SCI_Handle_t;

extern SCI_Handle_t SCIB;

void HAL_UART_Init(void);
void HAL_UART_SendPacket(void);
#endif
