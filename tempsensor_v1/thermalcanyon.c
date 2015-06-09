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
#include "main_system.h"

void thermal_handle_system_button() {
	if (!system_isRunning())
		return;

	if (g_iSystemSetup < 0)
		return;

	switch (g_iSystemSetup) {
	case 1:
		lcd_print("PRESS AGAIN");
		lcd_printl(LINE2, "TO SWAP SIM");
		break;
	case 2:
		modem_swap_SIM();
		break;
	case 3:
		lcd_print("PRESS AGAIN");
		lcd_printl(LINE2, "TO RE-CALIBRATE");
		break;
	case 4:
		g_iSystemSetup = -1;
		config_reconfigure();
		return;
	}

}

time_t thermal_update_time() {
	rtc_get(&g_tmCurrTime);
	return mktime(&g_tmCurrTime);
}

void thermal_low_battery_message() {
	lcd_turn_on();
	lcd_print("Low Battery     ");
	delay(HUMAN_DISPLAY_INFO_DELAY);
	lcd_turn_off();
	delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
}

void thermal_low_battery() {

	// If we have more than 10% of battery,
	// or we are connected to power, stay here until power is resumed.

	if ((g_iBatteryLevel > 10) || (P4IN & BIT4)) {
		return;
	}

	//Wait until battery is
	while (g_iBatteryLevel <= 10) {

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
	FRESULT fr;
	uint8_t iSampleCnt = 0;
	time_t currentTime = 0;
	uint8_t iDisplayId = 0;

	iUploadTimeElapsed = iMinuteTick;		//initialize POST minute counter
	iSampleTimeElapsed = iMinuteTick;
	iSMSRxPollElapsed = iMinuteTick;
	iLCDShowElapsed = iMinuteTick;
	iMsgRxPollElapsed = iMinuteTick;

	config_update_intervals();

	events_sync(thermal_update_time());
	lcd_show();

	while (1) {
		currentTime = thermal_update_time();
#ifdef _DEBUG
		events_debug(currentTime);
#endif

		// Checks all the events that we have and runs the right one.
		events_run(currentTime);

		//check if conversion is complete
		if (g_isConversionDone) {
			config_setLastCommand(COMMAND_MONITOR_ALARM);

			//convert the current sensor ADC value to temperature
			temperature_process_ADC_values();

#ifndef  NOTFROMFILE
			//log to file
			fr = log_sample_to_disk((int *) &iBytesLogged);
			if (fr == FR_OK) {
				iSampleCnt++;
				if (iSampleCnt >= MAX_NUM_CONTINOUS_SAMPLES) {
					g_iStatus |= LOG_TIME_STAMP;
					iSampleCnt = 0;
				}
			}
#endif
			//monitor for temperature alarms
			monitoralarm();
			g_isConversionDone = 0;

			if ((((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod)
					|| (g_iStatus & TEST_FLAG)
					|| (g_iStatus & BACKLOG_UPLOAD_ON)
					|| (g_iStatus & ALERT_UPLOAD_ON))
					&& !(g_iStatus & NETWORK_DOWN)) {

				data_transmit(&iSampleCnt);
			}
		}

#ifndef BUZZER_DISABLED
		if(g_iStatus & BUZZER_ON)
		{
			iIdx = 10;	//TODO fine-tune for 5 to 10 secs
			while((iIdx--) && (g_iStatus & BUZZER_ON))
			{
				P3OUT |= BIT4;
				delayus(1);
				P3OUT &= ~BIT4;
				delayus(1);
			}
		}
#endif

		//low power behavior
		thermal_low_battery();

		delay(MAIN_SLEEP_TIME);

		// Wait here behind the interruption to check for a change on display.
		// If a hardware button is pressed it will resume CPU

		if (iDisplayId != g_iDisplayId) {
			iDisplayId = g_iDisplayId;
			lcd_show();
		}

		__no_operation();                       // SET BREAKPOINT HERE
	}
}
