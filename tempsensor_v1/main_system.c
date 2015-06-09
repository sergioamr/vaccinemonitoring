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

int g_iRunning = 0;

char system_isRunning() {
	return g_iRunning;
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

	lcd_init();
	config_init(); // Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.
	config_setLastCommand(COMMAND_BOOT);

	g_iLCDVerbose = VERBOSE_BOOTING;         // Booting is not completed
	lcd_printf(LINEC, "Boot %d", (int) g_pSysCfg->numberConfigurationRuns);

#ifndef _DEBUG
	lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
#else
	lcd_printl(LINE2, "(db)" __TIME__);
#endif

	fat_init_drive();

	// Initial trigger of temperature required
	temperature_sample();

#ifndef _DEBUG
	// to allow conversion to get over and prevent any side-effects to other interface like modem
	// TODO is this delay to help on the following bug from texas instruments ? (http://www.ti.com/lit/er/slaz627b/slaz627b.pdf)

	lcd_progress_wait(2000);
#endif

	if (modem_first_init() != 1) {
		_NOP(); // Modem failed to power on
	}

	g_iBatteryLevel = batt_check_level();

	// Init finished, we disabled the debugging display
	g_iDisplayId = 0;
	lcd_disable_verbose();

	lcd_print("Finished Boot");
	log_appendf("Running tests");

	log_sample_web_format(&bytes_written);
	sms_process_messages(0);
}

_Sigfun * signal(int i, _Sigfun *proc) {
	__no_operation();
	return NULL;
}

/****************************************************************************/
/*  MAIN                                                                    */
/****************************************************************************/

int main(void) {

	WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT
	events_init();
	system_boot();
	g_iRunning = 1;		// System finished booting and we are going to run
	g_iSystemSetup = 0;	// Reset system setup button state

	thermal_canyon_loop();
}
