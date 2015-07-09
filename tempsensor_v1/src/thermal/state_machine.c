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
#include "state_machine.h"
#include "buzzer.h"
#include "alarms.h"

#define SYSTEM_SWITCH g_pSysState->system.switches
#define STATE_ALARM g_pSysState->state.alarms

// Overall state of the device to take decisions upon the state of the modem, storage, alerts, etc.

#pragma SET_DATA_SECTION(".state_machine")
SYSTEM_STATE g_SystemState;	// system states to control errors, connectivity, retries, etc
#pragma SET_DATA_SECTION()

SYSTEM_STATE *g_pSysState = &g_SystemState;

/*************************************************************************************************************/
/* General events that will generate responses from the system */
/*************************************************************************************************************/
uint8_t state_getSignalPercentage() {
	return ((float) (g_pSysState->signal_level - NETWORK_ZERO) * 100)
			/ (NETWORK_MAX_SS - NETWORK_ZERO);
}

/***********************************************************************************************************************/
/* STORAGE */
/***********************************************************************************************************************/

// There was a problem in the SD Card, reinit fat
void state_SD_card_problem(FRESULT fr, const char *szError) {
	if (fr != FR_OK) {
		// Activate try again later for the SD card to be re mounted.
		STATE_ALARM.SD_card_failure = STATE_ON;

		// Disable fat access while there is an error.
		g_bFatInitialized = false;
	}
}

void state_SD_card_OK() {
	STATE_ALARM.SD_card_failure = STATE_OFF;
}

void state_check_SD_card() {
	char msg[80];
	if (STATE_ALARM.SD_card_failure == STATE_ON) {
		fat_init_drive();

		if (g_bFatInitialized == true) {
			// Fat was recovered!
			STATE_ALARM.SD_card_failure = STATE_OFF;
			return;
		}

		if (!STATE_ALARM.globalAlarm) {
			state_alarm_on("SD FAILURE");
			if (g_szLastSD_CardError != NULL) {
				sprintf(msg, "%s SD error [%s]", g_pDevCfg->cfgIMEI,
						g_szLastSD_CardError);
				alarm_SD_card_failure(msg);
			}
		}
	}
}

void state_setSMS_notSupported(SIM_CARD_CONFIG *sim) {
	sim->SMSNotSupported = 1;
}

void state_sim_failure(SIM_CARD_CONFIG *sim) {
	// 69 - "Requested facility not implemented"
	// This cause indicates that the network is unable to provide the requested short message service.

	char line1[17];
	int t;
	int split = 0;

	// Split line looking for a space
	int len = strlen(sim->simLastError);
	if (len>14) {
		for (t=16; t>6; t--) {
			if (sim->simLastError[t]==' ') {
				split=t;
				break;
			}
		}

		if (split>0) {
			memcpy(line1, sim->simLastError, split);
			line1[split]=0;
			lcd_printl(LINEC, line1);
			lcd_printl(LINEE, &sim->simLastError[split+1]);
			return;
		}
	}

	lcd_printl(LINEC, "SIM ERROR");
	lcd_printl(LINEE, sim->simLastError);
}

/***********************************************************************************************************************/
/* NETWORK STATE AND STATUS */
/***********************************************************************************************************************/

uint8_t inline state_isSimOperational() {
	return g_pDevCfg->SIM[g_pDevCfg->cfgSIM_slot].simOperational;
}

void state_SIM_not_operational() {
	g_pDevCfg->SIM[g_pDevCfg->cfgSIM_slot].simOperational = 0;
}

void state_SIM_operational() {
	g_pDevCfg->SIM[g_pDevCfg->cfgSIM_slot].simOperational = 1;
}

NETWORK_SERVICE inline *state_getCurrentService() {
	if (g_pSysState->network_mode < 0 || g_pSysState->network_mode > 1)
		g_pSysState->network_mode = 0;

	return &g_pSysState->net_service[g_pSysState->network_mode];
}

char inline *state_getNetworkState() {
	NETWORK_SERVICE *service = state_getCurrentService();
	return service->network_state;
}

void state_setNetworkStatus(const char *status) {
	NETWORK_SERVICE *service = state_getCurrentService();
	zeroTerminateCopy(service->network_state, status);
}

uint8_t state_getSignalLevel() {
	return g_pSysState->signal_level;
}

int state_isSignalInRange() {
	int iSignalLevel = g_pSysState->signal_level;
	if ((iSignalLevel < NETWORK_DOWN_SS) || (iSignalLevel > NETWORK_MAX_SS)) {
		return 0;
	}
	return 1;
}

void state_setSignalLevel(uint8_t iSignal) {
	g_pSysState->signal_level = iSignal;
}

int state_isNetworkRegistered() {
	NETWORK_SERVICE *service = state_getCurrentService();

	if (service->network_status == NETWORK_STATUS_REGISTERED_HOME_NETWORK
			|| service->network_status == NETWORK_STATUS_REGISTERED_ROAMING)
		return 1;

	return 0;
}

/***********************************************************************************************************************/
/* GENERAL */
/***********************************************************************************************************************/

void state_init() {
	memset(g_pSysState, 0, sizeof(SYSTEM_STATE));

	g_pSysState->network_mode = NETWORK_NOT_SELECTED;
	SYSTEM_SWITCH.buzzer_disabled = BUZZER_DISABLE;

	if (POWER_ON)
		SYSTEM_SWITCH.power_connected = true;
	else
		SYSTEM_SWITCH.power_connected = false;
}

uint8_t state_isGPRS() {
	if (modem_getNetworkService() == NETWORK_GPRS)
		return true;

	return false;
}

uint8_t state_isGSM() {
	if (modem_getNetworkService() == NETWORK_GSM)
		return true;

	return false;
}

/***********************************************************************************************************************/
/* GENERATE ALARMS */
/***********************************************************************************************************************/

SYSTEM_SWITCHES *state_getSwitches() {
	return &g_pSysState->system;
}

SYSTEM_ALARMS *state_getAlarms() {
	return &g_pSysState->state;
}

void state_reset_sensor_alarm(int id) {
	int t;

	for (t = 0; t < SYSTEM_NUM_SENSORS; t++) {
		g_pSysState->temp.state[t].status = 0;
	}
}

void state_alarm_disable_buzzer_override() {
	SYSTEM_SWITCHES *s = state_getSwitches();
	s->switches.button_buzzer_override = false;
}

void state_alarm_enable_buzzer_override() {
	SYSTEM_SWITCHES *s = state_getSwitches();
	if (s->switches.buzzer_sound == STATE_ON)
		s->switches.button_buzzer_override = true;
}

void state_alarm_turnoff_buzzer() {
	SYSTEM_SWITCHES *s = state_getSwitches();
	s->switches.buzzer_sound = STATE_OFF;
	g_iStatus &= ~BUZZER_ON;
}

void state_alarm_turnon_buzzer() {
	buzzer_start();
	SYSTEM_SWITCHES *s = state_getSwitches();
	if (!s->switches.buzzer_disabled)
		s->switches.buzzer_sound = STATE_ON;
}

void state_alarm_on(char *alarm_msg) {
	SYSTEM_ALARMS *s = state_getAlarms();
	static uint16_t count = 0;
	uint16_t elapsed;

	// We are already in alarm mode
	if (s->alarms.globalAlarm == STATE_ON) {
		goto display_alarm;
	}

	s->alarms.globalAlarm = STATE_ON;
	state_alarm_turnon_buzzer();

#ifdef _DEBUG
	if (g_pDevCfg->cfg.logs.sms_reports)
		sms_send_message_number(g_pDevCfg->cfgReportSMS, alarm_msg);
#endif

	display_alarm:

	elapsed = events_getTick() - count;
	if (elapsed > 30) {
		lcd_clear();
		delay(500);
		lcd_turn_on();

		zeroTerminateCopy(g_pSysState->alarm_message, alarm_msg);
		events_display_alarm(NULL, 0);
		count = events_getTick();
	}
}

// Everything is fine!
void state_clear_alarm_state() {
	SYSTEM_ALARMS *s = state_getAlarms();

	//set buzzer OFF
	//reset alarm state and counters

	// We were not in alarm mode
	if (s->alarms.globalAlarm == STATE_OFF)
		return;

	if (g_pDevCfg->cfg.logs.sms_reports) {
		strcat(g_pSysState->alarm_message, " cleared");
		sms_send_message_number(g_pDevCfg->cfgReportSMS,
				g_pSysState->alarm_message);
	}

	g_pSysState->state.alarms.globalAlarm = STATE_OFF;
	SYSTEM_SWITCH.buzzer_sound = STATE_OFF;
}

/***********************************************************************************************************************/
/* MODEM & COMMUNICATIONS */
/***********************************************************************************************************************/

void state_SMS_lastMessageACK(SIM_CARD_CONFIG *sim, int8_t msgNumber) {
	sim->last_SMS_message = msgNumber;
}

void state_reset_network_errors() {
	uint8_t i;
	for (i = 0; i < SYSTEM_NUM_SIM_CARDS; i++) {
		g_pSysState->simState[i].failsGPRS = 0;
		g_pSysState->simState[i].failsGSM = 0;
	}
}

void state_network_status(int presentation_mode, int net_status) {
	NETWORK_SERVICE *service = state_getCurrentService();
	service->network_presentation_mode = presentation_mode;
	service->network_status = net_status;
}

// Clear all the errors for the network connection.
void state_network_success(uint8_t sim) {
	SIM_STATE *simState;

	if (sim > 1)
		return;

	simState = &g_pSysState->simState[sim];

	// Eveything is fine
	if (g_pSysState->network_mode == NETWORK_GSM)
		simState->failsGSM = 0;

	if (g_pSysState->network_mode == NETWORK_GPRS)
		simState->failsGPRS = 0;

	simState->modemErrors = 0;
}

// Checks several parameters to see if we have to reset the modem, switch sim card, etc.
void state_network_fail(uint8_t sim, uint16_t error) {
	state_SIM_not_operational();
}

void state_modem_timeout(uint8_t sim) {

}

void state_failed_gprs(uint8_t sim) {
	if (sim > 1)
		return;

	g_pSysState->simState[sim].failsGPRS++;
}

void state_failed_gsm(uint8_t sim) {
	if (sim > 1)
		return;

	g_pSysState->simState[sim].failsGSM++;
}

/***********************************************************************************************************************/
/* TEMPERATURE CHECKS */
/***********************************************************************************************************************/

/***********************************************************************************************************************/
/* BATTERY CHECKS */
/***********************************************************************************************************************/

void state_low_battery_alert() {

	if (STATE_ALARM.battery==true)
		return;

	STATE_ALARM.battery = true;

	// Activate sound alarm
	state_alarm_on("LOW BATTERY");
}

void state_battery_level(uint8_t battery_level) {

	g_pSysState->battery_level = battery_level;
	if (battery_level > g_pDevCfg->stBattPowerAlertParam.battThreshold)
		return;

	if (SYSTEM_SWITCH.power_connected == true)
		return;

	state_low_battery_alert();
	if (battery_level<BATTERY_HIBERNATE_THRESHOLD)
		thermal_low_battery_hibernate();
}

/***********************************************************************************************************************/
/* POWER CHECKS */
/***********************************************************************************************************************/

uint8_t state_isBuzzerOn() {
	// Manual override by the button
	if (SYSTEM_SWITCH.button_buzzer_override == true)
		return false;

	if (g_iStatus & BUZZER_ON)
		return true;

	return SYSTEM_SWITCH.buzzer_sound;
}

// Power out, we store the time in which the power went down
// called from the Interruption, careful

void state_power_out() {

	if (SYSTEM_SWITCH.power_connected == false)
		return;

	if (g_pSysState->time_powerOutage == 0)
		g_pSysState->time_powerOutage = rtc_get_second_tick();

	SYSTEM_SWITCH.power_connected = false;
	lcd_printl(LINEC, "POWER CABLE");
	lcd_printf(LINEE, "DISCONNECTED");
}

// called from the Interruption, careful
void state_power_on() {

	if (SYSTEM_SWITCH.power_connected == true)
		return;

	if (STATE_ALARM.poweroutage == STATE_ON) {
		sms_send_message_number(g_pDevCfg->cfgReportSMS, "POWER RESUMED");
		STATE_ALARM.poweroutage = STATE_OFF;
	}

	SYSTEM_SWITCH.power_connected = true;
	g_pSysState->time_powerOutage = 0;
	buzzer_feedback();

	// We reset the alarm from the battery since we are plugged in
	STATE_ALARM.battery = false;

	lcd_printl(LINEC, "POWER");
	lcd_printf(LINEH, "RESUMED");
}

void state_check_power() {

	if (POWER_ON)
		state_power_on();
	else
		state_power_out();

	if (SYSTEM_SWITCH.power_connected == true)
		return;

	if (!g_pDevCfg->stBattPowerAlertParam.enablePowerAlert)
		return;

	// Check the power down time to generate an alert

	if (STATE_ALARM.poweroutage || g_pDevCfg->stBattPowerAlertParam.minutesPower == 0)
		return;

	time_t currentTime = rtc_get_second_tick();
	time_t elapsed = currentTime - g_pSysState->time_powerOutage;

	if (elapsed > (g_pDevCfg->stBattPowerAlertParam.minutesPower) * 60) {
		STATE_ALARM.poweroutage = STATE_ON;
		alarm_sms_power_outage();
		state_alarm_on("POWER OUT");
	}
}

/***********************************************************************************************************************/
/* Check the state of every component */
/***********************************************************************************************************************/

void state_check_alarm_states() {
	int t;
	SYSTEM_ALARMS *s = state_getAlarms();
	SENSOR_STATUS *sensor;

	if (s->alarms.globalAlarm == STATE_OFF)
		return;

	if (s->alarms.battery || s->alarms.poweroutage || s->alarms.SD_card_failure)
		return;

	for (t = 0; t < SYSTEM_NUM_SENSORS; t++) {
		sensor = getAlarmsSensor(t);
		if (sensor->state.lowAlarm || sensor->state.highAlarm
				|| sensor->state.alarm)
			return;
	}

	// There is still an alarm in the system.
	state_clear_alarm_state();
}

void state_process() {

	static uint32_t last_check = 0;
	if (last_check == rtc_get_second_tick())
		return;
	last_check = rtc_get_second_tick();

	state_check_SD_card();

	// Global check for all the alarms
	state_check_alarm_states();
}
