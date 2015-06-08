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

void thermal_handle_system_button() {
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

void thermal_canyon_loop(void) {

	uint8_t iSampleCnt = 0;

	iUploadTimeElapsed = iMinuteTick;		//initialize POST minute counter
	iSampleTimeElapsed = iMinuteTick;
	iSMSRxPollElapsed = iMinuteTick;
	iLCDShowElapsed = iMinuteTick;
	iMsgRxPollElapsed = iMinuteTick;
	iBootTime = config_get_boot_midnight_difference();
	config_update_intervals();

	while (1) {

		if (((iMinuteTick - iLCDShowElapsed) >= LCD_REFRESH_INTERVAL)
				|| (iLastDisplayId != g_iDisplayId)) {

			config_setLastCommand(COMMAND_MAIN_LCD_DISPLAY);
			config_update_system_time();
			iLastDisplayId = g_iDisplayId;
			iLCDShowElapsed = iMinuteTick;
			lcd_show(g_iDisplayId);
		}

		if (iMinuteTick >= iBootTime) {
			modem_pull_time();
			config_setLastCommand(COMMAND_BOOT_MIDNIGHT);
			iBootTime = config_get_boot_midnight_difference();
		}

		//check if conversion is complete
		if ((g_isConversionDone) || (g_iStatus & TEST_FLAG)) {
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

#ifdef SMS
			memset(MSGData,0,sizeof(MSGData));
			strcat(MSGData,SensorName[g_iDisplayId]);
			strcat(MSGData," ");
			strcat(MSGData,Temperature);
			sendmsg(MSGData);
#endif

			g_isConversionDone = 0;

			if ((((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod)
					|| (g_iStatus & TEST_FLAG)
					|| (g_iStatus & BACKLOG_UPLOAD_ON)
					|| (g_iStatus & ALERT_UPLOAD_ON))
					&& !(g_iStatus & NETWORK_DOWN)) {

				data_transmit(&iSampleCnt);
				lcd_show(g_iDisplayId);	//remove the custom print (e.g transmitting)
			}
			P4IE |= BIT1;					// enable interrupt for button input
		}

		if ((iMinuteTick - iSampleTimeElapsed) >= g_iSamplePeriod) {
			config_setLastCommand(COMMAND_NETWORK_SIGNAL_MONITOR);
			iSampleTimeElapsed = iMinuteTick;
			P4IE &= ~BIT1;				// disable interrupt for button input
			temperature_sample();
			modem_check_network(); // Checks network and if it is down it does the swapping
		}

#ifndef CALIBRATION
		if ((iMinuteTick - iSMSRxPollElapsed) >= SMS_RX_POLL_INTERVAL) {

		}
#endif

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

#ifndef CALIBRATION
		//process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
		if ((iMinuteTick) && !(g_iStatus & BACKLOG_UPLOAD_ON)
				&& ((iMinuteTick - iSMSRxPollElapsed)
						< (SMS_RX_POLL_INTERVAL - 2))
				&& ((iMinuteTick - iUploadTimeElapsed) < (g_iUploadPeriod - 2))
				&& ((iMinuteTick - iMsgRxPollElapsed) >= MSG_REFRESH_INTERVAL)) {

			sms_process_messages(iMinuteTick);
			lcd_show(g_iDisplayId);
		}

		//low power behavior
		if ((g_iBatteryLevel <= 10) && (P4IN & BIT4)) {

			lcd_printl(LINEE, "Low Battery     ");

			//reset display
			//PJOUT |= BIT6;							// LCD reset pulled high
			//disable backlight
			lcd_bldisable();
			lcd_off();

			//loop for charger pluging
			while (g_iBatteryLevel <= 10) {
				if (iLastDisplayId != g_iDisplayId) {
					iLastDisplayId = g_iDisplayId;
					//enable backlight
					lcd_blenable();
					//lcd reset
					lcd_on();
					lcd_print("Low Battery     ");
					delay(HUMAN_DISPLAY_INFO_DELAY);
					//disable backlight
					lcd_bldisable();
					lcd_off();
					//reset lcd
					//PJOUT |= BIT6;							// LCD reset pulled high
				}

				//power plugged in
				if (!(P4IN & BIT4)) {
					lcd_blenable();
					lcd_on();
					lcd_print("Starting...");
					modem_init();
					iLastDisplayId = g_iDisplayId = 0;
					lcd_show(g_iDisplayId);
					break;
				}
			}

		}
#endif
		__no_operation();                       // SET BREAKPOINT HERE
	}
}
