/*
 * timer.c
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#include "config.h"
#include "timer.h"

volatile uint16_t iTick = 0;
volatile uint32_t delay_count = 0;

// Timer0_A0 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	if (++iTick >= delay_count)
		__bic_SR_register_on_exit(LPM0_bits); // Resume functionality.
}

void delay(uint32_t time) {
	iTick = 0;
	if (time > 180000 || time <= 10) {
		_NOP();
		time = 180000;
	}
	delay_count = time / 10;
	TA0CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA0CCR0 = 10000;					// 10ms (1 cnt = 1us @1MHz timer clk)
	TA0CTL = TASSEL__SMCLK | MC__UP | ID__8;  // SMCLK/8 (1MHz), UP mode

 	__bis_SR_register(LPM0_bits + GIE); // Disable CPU, keep functional master clock and slaves.

	TA0CTL = MC__STOP;
	iTick = 0;
}

