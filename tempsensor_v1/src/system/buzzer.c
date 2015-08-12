
#include "driverlib.h"
#include "thermalcanyon.h"
#include "state_machine.h"

#define TIMER_PERIOD 511
#define DUTY_CYCLE  350

#include "timer_b.h"
// 4Khz will be the loudest sound (accourding to documentation)

void buzzer_start() {

    GPIO_setAsOutputPin(
        GPIO_PORT_P3,
        GPIO_PIN4
        );

	TA2CCTL0 = CCIE;

	if (!g_pSysState->system.switches.buzzer_sound && g_pSysState->buzzerFeedback>0)
		TA2CCR0 = 8000;						  // 0.5khz normal op

	else
		TA2CCR0 = 1000;						  // 4khz alarm op
	TA2CTL = TASSEL__SMCLK | MC__UP | ID__2;  // SMCLK/2 (4MHz), UP mode

}

void buzzer_feedback_value(uint16_t value) {
	//disable buzzer if system switch is set, else play sound
	if(g_pSysState->system.switches.buzzer_disabled != 1){
		g_pSysState->buzzerFeedback = value;
		buzzer_start();
	}
}

void buzzer_feedback() {
	buzzer_feedback_value(50);
}

/*
void buzzer_feedback_simple() {
	buzzer_feedback_value(50);
}
*/

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
	static uint8_t count = 0;

	if (g_pSysState->buzzerFeedback>0) {
	    GPIO_toggleOutputOnPin(
	        GPIO_PORT_P3,
	        GPIO_PIN4);
		g_pSysState->buzzerFeedback--;
		return;
	}

	if (count < 2) {
		GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN4);
	} else
	if (count == 3) {
		GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN4);
		count = 0;
	}

	count++;

	if (!state_isBuzzerOn()) {
		TA2CTL = MC__STOP;
		return;
	}
}

