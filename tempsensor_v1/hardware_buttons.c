/*
 * buttons.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */
#include "thermalcanyon.h"
#include "state_machine.h"
#include "hardware_buttons.h"
#include "state_machine.h"

void thermal_handle_system_button();
// Interrupt services

uint8_t switch_check_service_pressed() {
	return !(P3IN&BIT5);
}

void switchers_setupIO() {
	// Setup buttons
	P4IE |= BIT1;							// enable interrupt for button input
	P4IE |= BIT4;							// enable interrupt for Power input
	P2IE |= BIT2;							// enable interrupt for buzzer off
	P3IE |= BIT5;							// enable interrupt for extra button setup
}

void hardware_disable_buttons() {
	P4IE &= ~BIT1;				// disable interrupt of button input
	P3IE &= ~BIT5;				// enable interrupt for extra button setup
}

void hardware_enable_buttons() {
	switchers_setupIO();
}

// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) Port_2 (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(P2IV, P2IV_P2IFG7)) {
	case P2IV_NONE:
		break;
	case P2IV_P2IFG0:
		break;
	case P2IV_P2IFG1:
		break;
	case P2IV_P2IFG2:
		//P3OUT &= ~BIT4;                           // buzzer off
		g_iStatus &= ~BUZZER_ON;
#ifdef _DEBUG
		g_iDebug = 1;
		g_iSystemSetup = 0;
#endif
		event_LCD_turn_on();
		break;
	default:
		break;
	}
}

// Port 3 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT3_VECTOR
__interrupt void Port_3(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT3_VECTOR))) Port_3 (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(P3IV, P3IV_P3IFG5)) {
	case P4IV_NONE:
		break;
	case P3IV_P3IFG5:
		g_iSystemSetup ++;
		thermal_handle_system_button();
		event_LCD_turn_on();
		__bic_SR_register_on_exit(LPM0_bits); // Resume execution if we are sleeping
		break;
	default:
		break;
	}
}

// Port 4 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) Port_4 (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(P4IV, P4IV_P4IFG7)) {
	case P4IV_NONE:
		break;
	case P4IV_P4IFG0:
		break;
	case P4IV_P4IFG4:
		if (POWER_ON)
			state_power_on();
		else
			state_power_out();

		event_force_event_by_id(EVT_ALARMS_CHECK, 0);
		break;
	case P4IV_P4IFG1:
		g_iDisplayId++;
		g_iDisplayId %= MAX_DISPLAY_ID;
		event_LCD_turn_on();
		__bic_SR_register_on_exit(LPM0_bits); // Resume execution if we are sleeping
		break;
	default:
		break;
	}
}

