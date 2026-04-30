#include <msp430.h>
#include "hal_adc.h"

/**
 * @brief ADC10 Donanımını ilklendirir ve kesmeleri açar.
 */
void HAL_ADC_Init(void) {
    // Kanal 0 (INCH_0) seçili, ADC10 dahili saat (ADC10SSEL_3)
    ADC10CTL1 = INCH_0 + ADC10SSEL_3;
    
    // SREF_0: Vcc/Vss referans
    // ADC10ON: ADC Açık
    // ADC10IE: Kesme Yetkilendirme (Kritik!)
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;
    
    // P1.0 pinini ADC girişi olarak işaretle
    ADC10AE0 |= BIT0;
}

/**
 * @brief ADC Okumasını başlatır ve işlemciyi uyutur.
 * Veri hazır olduğunda ISR işlemciyi uyandırır ve sonuç döndürülür.
 */
uint16_t HAL_ADC_Read(void) {
    // Okumayı yetkilendir ve başlat
    ADC10CTL0 |= ENC + ADC10SC;
    
    // ULP 3.1 Çözümü: 
    // Ölçüm bitene kadar işlemciyi LPM0 (CPUOFF) moduna al ve global kesmeleri aç.
    // Bu sayede bekleme sırasında 0 Watt'a yakın enerji harcanır.
    __bis_SR_register(CPUOFF + GIE); 
    
    // Kesme bizi uyandırdığına göre veri hazırdır
    return ADC10MEM;
}

/**
 * @brief ADC10 Kesme Servis Rutini
 * Dönüşüm tamamlandığında donanım burayı tetikler.
 */
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
    // Ölçüm bitti, ana programdaki HAL_ADC_Read içindeki uyku modundan çık
    __bic_SR_register_on_exit(CPUOFF);
}
