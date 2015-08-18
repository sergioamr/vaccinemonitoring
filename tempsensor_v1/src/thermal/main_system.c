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

	/*
#ifndef _DEBUG
	lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
#else
	lcd_printl(LINE2, "(db)" __TIME__);
#endif
*/

	// Initial trigger of temperature capture. Just get one sample
	temperature_subsamples(1);
	temperature_single_capture();
#ifndef _DEBUG
	// to allow conversion to get over and prevent any side-effects to other interface like modem
	// TODO is this delay to help on the following bug from texas instruments ? (http://www.ti.com/lit/er/slaz627b/slaz627b.pdf)

	lcd_progress_wait(2000);
#endif

	modem_first_init();
	/*
	if (modem_first_init() != 1) {
		_NOP(); // Modem failed to power on
	}
	*/

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

// Paints the stack in a value to check for anomalies
//#define EMPTY_STACK_VALUE 0x69

//uint8_t stackClear = EMPTY_STACK_VALUE;
#ifdef ___CHECK_STACK___
// Used to check the stack for leaks
extern char __STACK_END;
extern char __STACK_SIZE;

void checkStack() {
	char *pStack = (void*) (&__STACK_END - &__STACK_SIZE);
	size_t stack_empty = 0;
	char *current_SP;

	current_SP = (char *) _get_SP_register();

	while (current_SP > pStack) {
		current_SP --;
		*current_SP = stackClear;
		stack_empty++;
	}

	if (g_pSysCfg->stackLeft==0 || stack_empty < g_pSysCfg->stackLeft)
		g_pSysCfg->stackLeft = stack_empty;

	if (stack_empty == 0) {
		*current_SP = 0;
	}

	stackClear++;
}
#endif

 int main(void) {

	// Disable for init since we are not going to be able to respond to it.
	watchdog_disable();

	state_init();  // Clean the state machine
	system_boot();

	events_init();
	state_process();

	// Done init, start watchdog
	watchdog_init();

	events_sync(rtc_update_time());
	lcd_show();

	while (1) {
#ifdef ___CHECK_STACK___
		checkStack();
#endif
		/*
#ifdef _DEBUG
		//events_debug(rtc_get_second_tick());
#endif
*/
		hardware_actions();
		// Checks all the events that we have and runs the right one.
		events_run(rtc_update_time());

		// Wait here behind the interruption to check for a change on display.
		// If a hardware button is pressed it will resume CPU
		event_main_sleep();
	}
}
