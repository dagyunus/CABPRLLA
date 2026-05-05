#include <msp430.h>
#include "hal_gpio.h"

void HAL_GPIO_InitClock(void) {
    BCSCTL1 = RSEL2 | RSEL1 | RSEL0; 
    DCOCTL = DCO1 | DCO0; 
}

void HAL_GPIO_InitPins(void) {
    // 1. Port 1: Girişler (P1.0 - P1.4)
    P1DIR &= ~0x1F; 
    P1REN |= 0x1F; 
    P1OUT |= 0x1F; 
    P1IE  = 0; // TÜM KESMELERİ KAPATTIK (Risk sıfırlandı)

    // 2. Port 2: TÜMÜ GİRİŞ (Sensörler ve Kristal güvende)
    P2DIR = 0x00; 
    P2SEL = 0x00;
    
    // 3. Port 3: UART
    P3SEL |= BIT4 | BIT5; 
    
    // 4. Port 4: SADECE GEREKLİ ÇIKIŞLAR (Röleler ve RS485)
    // P4.0-P4.4 Röleler, P4.7 RS485
    P4DIR = (BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT7);
    P4DIR |= 0X9F; // 1001-1111
    P4OUT &= ~0x1F; // İlk 5 biti (0001 1111) SIFIRLA -> Tüm röleler kapandı.
    P4OUT |=  0x80; // En üst biti (1000 0000) BİR YAP -> RS485 Dinleme moduna geçti.
}

void HAL_GPIO_RelaySet(uint32_t val) {
    P4OUT = (P4OUT & 0xE0) | (uint8_t)(val & 0x1F);
}
