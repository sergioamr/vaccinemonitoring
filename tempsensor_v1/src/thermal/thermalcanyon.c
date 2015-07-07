/* Copyright (c) 2015, Intel Corporation. All rights reserved.
 *
 * INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL PRODUCTS. NO LICENSE, EXPRESS OR IMPLIED,
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

void thermal_low_battery_message() {
	lcd_turn_on();
	lcd_printl(LINEC, "Low Battery");
	lcd_printl(LINEC, "Hibernating...");
	delay(HUMAN_DISPLAY_INFO_DELAY);
	lcd_turn_off();
	delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
}

void thermal_low_battery_hibernate() {

	// If we have more than 10% of battery,
	// or we are connected to power, stay here until power is resumed.

	//Wait until battery is
	while (batt_check_level() <= BATTERY_HIBERNATE_THRESHOLD) {

		thermal_low_battery_message();
		//power plugged in
		if (!(P4IN & BIT4)) {
			lcd_turn_on();
			lcd_print("Recovery...");
			modem_init();
			lcd_show();
			return;
		}
	}
}

void thermal_canyon_loop(void) {
	time_t currentTime = 0;

	events_sync(rtc_update_time());
	event_force_event_by_id(EVT_CHECK_NETWORK, 0);
	lcd_show();

	while (1) {
		// Checks if the current sim is the selected one.
		modem_check_sim_active();

		currentTime = rtc_update_time();
#ifdef _DEBUG
		events_debug(rtc_get_second_tick());
#endif
		hardware_actions();
		// Checks all the events that we have and runs the right one.
		events_run(currentTime);

		// Wait here behind the interruption to check for a change on display.
		// If a hardware button is pressed it will resume CPU
		event_main_sleep();
	}
}
