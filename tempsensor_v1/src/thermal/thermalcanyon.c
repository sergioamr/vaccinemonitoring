#include "thermalcanyon.h"
#include "data_transmit.h"
#include "hardware_buttons.h"
#include "fatdata.h"
#include "main_system.h"
#include "alarms.h"
#include "state_machine.h"
#include "wdt_a.h"

void thermal_handle_system_button() {
/*
	if (!g_iRunning)
		return;

	if (g_iSystemSetup < 0)
		return;

	switch (g_iSystemSetup) {
	case 1:
		lcd_printl(LINEC, "PRESS AGAIN");
		lcd_printl(LINE2, "TO SWAP SIM");
		break;
	case 2:
		modem_swap_SIM();
		break;
	case 3:
		lcd_printl(LINEC, "PRESS AGAIN");
		lcd_printl(LINE2, "TO RE-CALIBRATE");
		break;
	case 4:
		g_iSystemSetup = -1;
		config_reconfigure();
		return;
	}
*/
}

void thermal_low_battery_message(uint8_t firstWarning) {
	lcd_turn_on();
	if (firstWarning) {
		lcd_printl(LINEC, "Low Battery");
		lcd_printl(LINE2, "Hibernating...");
	}
	delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
	lcd_turn_off();
	delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
}

void thermal_low_battery_hibernate() {
	uint8_t firstWarning = 1;
	// If we have more than 10% of battery,
	// or we are connected to power, stay here until power is resumed.

	//Wait until battery is
	while (batt_check_level() <= BATTERY_HIBERNATE_THRESHOLD) {
		thermal_low_battery_message(firstWarning);
		// TODO: perhaps shut down the modem?
		firstWarning = 0;
		//power plugged in
		if (g_pSysState->system.switches.power_connected) {
			lcd_turn_on();
			lcd_print("Recovering...");
			modem_init();
			lcd_show();
			return;
		}
	}
}

