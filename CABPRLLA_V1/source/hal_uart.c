#include <msp430.h>
#include "hal_uart.h"
#include "hal_gpio.h"
#include "modbus.h"

SCI_Handle_t SCIB;

void HAL_UART_Init(void) {
    UCA0CTL1 |= UCSWRST | UCSSEL_2;
    UCA0BR0 = 10; UCA0BR1 = 0; UCA0MCTL = UCBRS1;
    UCA0CTL1 &= ~UCSWRST;
    
    IE2 |= UCA0RXIE;
    
    // Timer_A Kesmesi Aktif (Gecikmeler sırasında uyumak için)
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL_2 + MC_0; 
}

void HAL_UART_SendPacket(void) {
    SCIB.SentData.Counter = 0;
    RS485_TX();
    
    // ULP 3.1 Çözümü: while bayrak beklemek yerine Timer kur ve uyu
    TA0CCR0 = 1000; 
    TA0CTL = TASSEL_2 + TACLR + MC_1;
    __bis_SR_register(LPM0_bits + GIE); // Timer uyarana kadar derin uyku
    
    // Uyandık (Timer kesmesi bizi uyandırdı), veriyi basmaya başla
    UCA0TXBUF = SCIB.SentData.Data[SCIB.SentData.Counter++];
    IE2 |= UCA0TXIE; 
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
    uint8_t d = UCA0RXBUF;
    if (SCIB.RecData.Counter == 0 && d != DEVICE_ID) return;
    if (SCIB.RecData.Counter < BUFFER_SIZE) {
        SCIB.RecData.Data[SCIB.RecData.Counter++] = d;
        SCIB.RecData.Idle = 0;
    }
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void) {
    if (SCIB.SentData.Counter < SCIB.SentData.Length) {
        UCA0TXBUF = SCIB.SentData.Data[SCIB.SentData.Counter++];
    } else {
        IE2 &= ~UCA0TXIE; // TX bitti
        
        // Son byte'ın çıkış süresini (UCBUSY beklemek yerine) Timer ile uyu
        TA0CCR0 = 2500; // ~2.5ms güvenli bekleme (Profesyonel Post-delay)
        TA0CTL = TASSEL_2 + TACLR + MC_1;
        // Burada uykuya dalmıyoruz çünkü ISR içindeyiz, Timer kesmesi gelince pini değiştirecek.
    }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A_ISR(void) {
    TA0CTL = MC_0; // Timer'ı durdur
    
    // DÜZELTME: 
    // Sadece paket gönderimi BİTTİYSE (Counter == Length) RX'e dön.
    // Eğer paket daha başlamamışsa (Counter == 0) bu bir Pre-delay'dir, RX'e dönme!
    if (!(P4OUT & BIT7) && (SCIB.SentData.Counter >= SCIB.SentData.Length)) {
        RS485_RX();
    }
    
    // Ana döngüyü uyandır (LPM'den çık)
    __bic_SR_register_on_exit(LPM0_bits);
}
