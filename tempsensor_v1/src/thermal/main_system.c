/* Copyright (c) 2015, Intel Corporation. All rights reserved.
 *
 * INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL� PRODUCTS. NO LICENSE, EXPRESS OR IMPLIED,
 * BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS IS GRANTED BY THIS DOCUMENT. EXCEPT AS PROVIDED
 * IN INTEL'S TERMS AND CONDITIONS OF SALE FOR SUCH PRODUCTS, INTEL ASSUMES NO LIABILITY WHATSOEVER, AND INTEL
 * DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR USE OF INTEL PRODUCTS INCLUDING LIABILITY
 * OR WARRANTIES RELATING TO FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT OF ANY PATENT,
 * COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.
 * UNLESS OTHERWISE AGREED IN WRITING BY INTEL, THE INTEL PRODUCTS ARE NOT DESIGNED NOR INTENDED FOR ANY APPLICATION
 * IN WHICH THE FAILURE OF THE INTEL PRODUCT COULD CREATE A SITUATION WHERE PERSONAL INJURY OR DEATH MAY OCCUR.
 *
 * Intel may make changes to specifications and product descriptions at any time, without notice.
 * Designers must not rely on the absence or characteristics of any features or instructions marked
 * "reserved" or "undefined." Intel reserves these for future definition and shall have no responsibility
 * whatsoever for conflicts or incompatibilities arising from future changes to them. The information here
 * is subject to change without notice. Do not finalize a design with this information.
 * The products described in this document may contain design defects or errors known as errata which may
 * cause the product to deviate from published specifications. Current characterized errata are available on request.
 *
 * This document contains information on products in the design phase of development.
 * All Thermal Canyons featured are used internally within Intel to identify products
 * that are in development and not yet publicly announced for release.  Customers, licensees
 * and other third parties are not authorized by Intel to use Thermal Canyons in advertising,
 * promotion or marketing of any product or services and any such use of Intel's internal
 * Thermal Canyons is at the sole risk of the user.
 *
 */

#include "thermalcanyon.h"
#include "data_transmit.h"
#include "hardware_buttons.h"
#include "fatdata.h"
#include "events.h"
#include "state_machine.h"
#include "main_system.h"
#include "temperature.h"
#include "watchdog.h"

int g_iRunning = 0;

//reset the board by issuing a SW BOR
void system_reboot(const char *message) {
	log_appendf("REBOOT %s", message);
	delay(1000);
	PMM_trigBOR();
	while (1);	//code should not come here
}

/****************************************************************************/
/*  IO SETUP                                                                */
/****************************************************************************/

// Setup Pinout for I2C, and SPI transactions.
// http://www.ti.com/lit/ug/slau535a/slau535a.pdf
void system_setupIO_clock() {
	// Startup clock system with max DCO setting ~8MHz
	CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
	CSCTL1 = DCOFSEL_3 | DCORSEL;             // Set DCO to 8MHz
	CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
	//CSCTL3 = DIVA__1 | DIVS__8 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL4 &= ~LFXTOFF;
	do {
		CSCTL5 &= ~LFXTOFFG;                    // Clear XT1 fault flag
		SFRIFG1 &= ~OFIFG;
	} while (SFRIFG1 & OFIFG);                   // Test oscillator fault flag
	CSCTL0_H = 0;                             // Lock CS registers
}

static void system_setupIO_SPI_I2C() {
	P2DIR |= BIT3;							// SPI CS
	P2OUT |= BIT3;					// drive SPI CS high to deactive the chip
	P2SEL1 |= BIT4 | BIT5 | BIT6;             // enable SPI CLK, SIMO, SOMI
	PJSEL0 |= BIT4 | BIT5;                    // For XT1
	P1SEL1 |= BIT6 | BIT7;					// Enable I2C SDA and CLK
}

static void setup_IO() {

	// Configure GPIO
	system_setupIO_SPI_I2C();
	uart_setupIO();
	switchers_setupIO();

	P3DIR |= BIT4;      						// Set P3.4 buzzer output

	lcd_setupIO();

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	system_setupIO_clock();
	uart_setupIO_clock();
	ADC_setupIO();

#ifndef I2C_DISABLED
	//i2c_init(EUSCI_B_I2C_SET_DATA_RATE_400KBPS);
	i2c_init(380000);
#endif

	__bis_SR_register(GIE);		//enable interrupt globally
}

/****************************************************************************/
/*  SYSTEM START AND CONFIGURATION                                          */
/****************************************************************************/

void modem_turn_on() {
	// Turn modem ON
	P4OUT &= ~BIT0;
}

void modem_turn_off() {
	// Turn modem ON
	P4OUT |= BIT0;
}

void system_boot() {
	UINT bytes_written = 0;

	// Turn off modem
	modem_turn_off();
	setup_IO();

	lcd_reset();
	lcd_blenable(); // For some reason, lcd has to be enabled before battery init. Probably something with the i2c, still checking what is the problem here.

#ifndef BATTERY_DISABLED
	batt_init();
#endif
	modem_turn_on();

	hardware_enable_buttons();
	lcd_init();
	fat_init_drive();

	config_init(); // Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.
	config_setLastCommand(COMMAND_BOOT);

	g_iLCDVerbose = VERBOSE_BOOTING;         // Booting is not completed
	lcd_printf(LINEC, "Boot %d", (int) g_pSysCfg->numberConfigurationRuns);

#ifndef _DEBUG
	lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
#else
	lcd_printl(LINE2, "(db)" __TIME__);
#endif

	// Initial trigger of temperature capture. Just get one sample
	temperature_subsamples(1);
	temperature_single_capture();
#ifndef _DEBUG
	// to allow conversion to get over and prevent any side-effects to other interface like modem
	// TODO is this delay to help on the following bug from texas instruments ? (http://www.ti.com/lit/er/slaz627b/slaz627b.pdf)

	lcd_progress_wait(2000);
#endif

	if (modem_first_init() != 1) {
		_NOP(); // Modem failed to power on
	}

	batt_check_level();

	// Init finished, we disabled the debugging display
	g_iDisplayId = 0;
	lcd_disable_verbose();

	log_appendf("Running tests");

	temperature_analog_to_digital_conversion();
	log_sample_web_format(&bytes_written);
	sms_process_messages();

	//#ifndef _DEBUG
	backend_get_configuration();
	//#endif

	lcd_print("Finished Boot");

	g_iRunning = 1;		// System finished booting and we are going to run
	g_iSystemSetup = 0;	// Reset system setup button state
}

_Sigfun * signal(int i, _Sigfun *proc) {
	__no_operation();
	return NULL;
}

/****************************************************************************/
/*  MAIN                                                                    */
/****************************************************************************/

#define EMPTY_STACK_VALUE 0x69

#ifdef ___CHECK_STACK___
// Used to check the stack for leaks
extern char __STACK_END;
extern char __STACK_SIZE;

void checkStack() {
	size_t stack_size = (size_t) (&__STACK_SIZE);
	char *pStack = (void*) (&__STACK_END - &__STACK_SIZE);
	size_t t;
	size_t stack_empty = 0;

	for (t = 0; t < stack_size; t++, pStack++) {
		if (*pStack != EMPTY_STACK_VALUE)
			break;

		stack_empty++;
	}
	g_pSysCfg->stackLeft = stack_empty;

	if (stack_empty<64)
		sms_send_message_number(g_pDevCfg->cfgReportSMS, "Low Stack!");

	if (g_pSysCfg->stackLeft<64) {
		_NOP();
	}
}

void clearStack() {
	register char *pStack = (void*) (&__STACK_END);
	register size_t t;
	for (t = 0; t < (size_t) (&__STACK_SIZE); t++) {
		pStack--;
		*pStack = EMPTY_STACK_VALUE;
	}
	_NOP();
}
#endif

int main(void) {
#ifdef ___CHECK_STACK___
	clearStack();
#endif

	// Disable for init since we are not going to be able to respond to it.
	watchdog_disable();

	state_init();  // Clean the state machine
	system_boot();

#ifdef ___CHECK_STACK___
	checkStack();
#endif

	events_init();
	state_process();

	// Done init, start watchdog
	watchdog_init();

#ifdef _DEBUG
	if (g_pDevCfg->cfg.logs.sms_reports)
		sms_send_message_number(g_pDevCfg->cfgReportSMS, "Boot completed");
#endif

	thermal_canyon_loop();
}
