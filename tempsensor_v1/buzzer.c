
#include "driverlib.h"
#include "thermalcanyon.h"
#include "state_machine.h"

#define TIMER_PERIOD 511
#define DUTY_CYCLE  350

#include "timer_B.h"
// 4Khz will be the loudest sound (accourding to documentation)

void buzzer_start() {

    GPIO_setAsOutputPin(
        GPIO_PORT_P3,
        GPIO_PIN4
        );

	TA2CCTL0 = CCIE;

	if (g_pSysState->buzzerFeedback>0)
		TA2CCR0 = 8000;						  // 0.5khz
	else
		TA2CCR0 = 1000;						  // 4khz
	TA2CTL = TASSEL__SMCLK | MC__UP | ID__2;  // SMCLK/2 (4MHz), UP mode

}

void buzzer_feedback() {
	g_pSysState->buzzerFeedback = 50;
	buzzer_start();
}

void buzzer_feedback_simple() {
	g_pSysState->buzzerFeedback = 10;
	buzzer_start();
}

// Timer1_A0 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer2_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER2_A0_VECTOR))) Timer2_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
    GPIO_toggleOutputOnPin(
        GPIO_PORT_P3,
        GPIO_PIN4);

	if (g_pSysState->buzzerFeedback>0) {
		g_pSysState->buzzerFeedback--;
		return;
	}

	if (!state_isBuzzerOn()) {
		TA2CTL = MC__STOP;
		return;
	}
}
