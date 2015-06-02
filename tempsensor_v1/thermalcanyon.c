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

void thermal_canyon_loop(void) {

	uint32_t iIdx = 0;
	uint8_t iSampleCnt = 0;
	char gprs_network_indication = 0;

	iUploadTimeElapsed = iMinuteTick;		//initialize POST minute counter
	iSampleTimeElapsed = iMinuteTick;
	iSMSRxPollElapsed = iMinuteTick;
	iLCDShowElapsed = iMinuteTick;
	iMsgRxPollElapsed = iMinuteTick;

	while (1) {

		if (g_iSystemSetup > 0) {
			lcd_clear();
			lcd_print("RUN CALIBRATION?");
			lcd_print("PRESS AGAIN TO RUN");
			g_iSystemSetup = 0;
		}

		//check if conversion is complete
		if ((g_isConversionDone) || (g_iStatus & TEST_FLAG)) {
			//convert the current sensor ADC value to temperature
			for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
				memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);
				digital_amp_to_temp_string(ADCvar[iIdx], &Temperature[iIdx][0],
						iIdx);
			}

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
			iSampleTimeElapsed = iMinuteTick;
			P4IE &= ~BIT1;				// disable interrupt for button input
			sample_temperature();
			delay(2000);	//to allow the ADC conversion to complete

			if (g_iStatus & NETWORK_DOWN) {
				delay(2000);//additional delay to enable ADC conversion to complete
				//check for signal strength

				modem_pull_time();
				modem_checkSignal();
				if ((iSignalLevel > NETWORK_DOWN_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					//send heartbeat
					sms_send_heart_beat();
				}
			}

		}

#ifndef CALIBRATION
		if ((iMinuteTick - iSMSRxPollElapsed) >= SMS_RX_POLL_INTERVAL) {
			iSMSRxPollElapsed = iMinuteTick;
			lcd_clear();
			lcd_print_lne(LINE1, "Configuring...");

			modem_checkSignal();
			g_iSignal_gprs = dopost_gprs_connection_status(GPRS);

			iIdx = 0;
			while (iIdx < MODEM_CHECK_RETRY) {
				gprs_network_indication = dopost_gprs_connection_status(GSM);
				if ((gprs_network_indication == 0)
						|| ((iSignalLevel < NETWORK_DOWN_SS)
								|| (iSignalLevel > NETWORK_MAX_SS))) {
					lcd_print_lne(LINE2, "Signal lost...");
					g_iStatus |= NETWORK_DOWN;
					//iOffset = 0;  // Whyy??? whyyy?????
					modem_init();
					lcd_print_lne(LINE2, "Reconnecting...");
					iIdx++;
				} else {
					g_iStatus &= ~NETWORK_DOWN;
					break;
				}
			}

			if (iIdx == MODEM_CHECK_RETRY) {
				modem_swap_SIM();
			}

			//sms config reception and processing
			dohttpsetup();
			memset(ATresponse, 0, sizeof(ATresponse));
			doget(ATresponse);
			if (ATresponse[0] == '$') {
				if (sms_process_msg(ATresponse)) {
					//send heartbeat on successful processing of SMS message
					sms_send_heart_beat();
				}
			} else {
				// no cfg message recevied
				// check signal strength
				// iOffset = -1; //reuse to indicate no cfg msg was received // Whyy??? whyyy?????
			}
			deactivatehttp();
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

		if (((iMinuteTick - iLCDShowElapsed) >= LCD_REFRESH_INTERVAL)
				|| (iLastDisplayId != g_iDisplayId)) {
			iLastDisplayId = g_iDisplayId;
			iLCDShowElapsed = iMinuteTick;
			lcd_show(g_iDisplayId);
		}
		//iTimeCnt++;

#ifndef CALIBRATION
		//process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
		if ((iMinuteTick) && !(g_iStatus & BACKLOG_UPLOAD_ON)
				&& ((iMinuteTick - iSMSRxPollElapsed)
						< (SMS_RX_POLL_INTERVAL - 2))
				&& ((iMinuteTick - iUploadTimeElapsed) < (g_iUploadPeriod - 2))
				&& ((iMinuteTick - iMsgRxPollElapsed) >= MSG_REFRESH_INTERVAL)) {

			sms_process_messages(iMinuteTick, g_iDisplayId);
		}

		//low power behavior
		if ((g_iBatteryLevel <= 10) && (P4IN & BIT4)) {
			lcd_clear();
			lcd_print("Low Battery     ");
			delay(HUMAN_DISPLAY_INFO_DELAY);
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
					lcd_clear();
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
