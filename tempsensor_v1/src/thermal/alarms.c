/*
 * monitor_alarm.c
 *
 *  Created on: Jun 2, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "sms.h"
#include "globals.h"
#include "temperature.h"
#include "events.h"

#define SMS_ALERT

/*************************************************************************************************************/
/* Monitor alarm */
/*************************************************************************************************************/

/*************************************************************************************************************/
void alarm_sms_battery_level() {
	char *msg = getSMSBufferHelper();
	int i;

	if (!g_pDevCfg->cfg.logs.sms_alerts)
		return;

	// Send to all SMS numbers
	for (i = 0; i < MAX_SMS_NUMBERS; i++){
		sms_send_message_number(g_pDevCfg->cfgSMSNumbers[i].cfgReportSMS, "Low Battery!");
	}

}

void alarm_sms_sensor(uint8_t sensorId, int elapsed) {
	char *msg = getSMSBufferHelper();
	int i;

	if (!g_pDevCfg->cfg.logs.sms_alerts)
		return;

	sprintf(msg, "Alert Sensor %s at %s degC for %d mins", SensorName[sensorId],
			temperature_getString(sensorId), elapsed / 60);
	strcat(msg, " Take ACTION immediately.");

	// Send to all SMS numbers
	for (i = 0; i < MAX_SMS_NUMBERS; i++){
		sms_send_message_number(g_pDevCfg->cfgSMSNumbers[i].cfgReportSMS, msg);
	}
}

void alarm_sms_power_outage() {
	int i;

	if (!g_pDevCfg->cfg.logs.sms_alerts)
		return;

	// Send to all SMS numbers
	for (i = 0; i < MAX_SMS_NUMBERS; i++){
		sms_send_message_number(g_pDevCfg->cfgSMSNumbers[i].cfgReportSMS, "Power Outage! ATTENTION!");
	}
}

void alarm_SD_card_failure(char *msg) {

	if (!g_pDevCfg->cfg.logs.sms_alerts)
		return;
}

SENSOR_STATUS *getAlarmsSensor(int id) {
	USE_TEMPERATURE
	return &tem->state[id];
}

void alarm_test_sensor(int id) {
	char msg[32];
	char tmp[TEMP_DATA_LEN+1];
	USE_TEMPERATURE
	SENSOR_STATUS *s = getAlarmsSensor(id);
	TEMPERATURE_SENSOR *sensor = &tem->sensors[id];
	time_t elapsed = 0;
	memset(tmp,0,sizeof(tmp));

	float cold = g_pDevCfg->stTempAlertParams[id].threshCold;
	float hot = g_pDevCfg->stTempAlertParams[id].threshHot;

	float temperature = sensor->fTemperature;
	if (temperature == 0) {
		if (s->state.disconnected == false) {
			s->state.disconnected = true;
			sprintf(msg, "%s disconnected", SensorName[id]);
			if (s->state.wasConnected == true) {
				goto alarm_error;
			}
		}
		return;
	}

	if (temperature > cold && temperature < hot) {
		if (s->state.alarm) {
			s->status = STATUS_NO_ALARM;
			tem->alarm_time = 0;
			g_pSysState->system.switches.button_buzzer_override = false;
			log_appendf("Alarm %d OK", id);
		}
		return;
	}

	if (s->state.alarm)
		return;

	if (tem->alarm_time == 0)
		tem->alarm_time = events_getTick();
	else
		elapsed = events_getTick() - tem->alarm_time;

	//lcd_printf(LINEC, "Sensor \"%s\" %s", SensorName[id], getFloatNumber2Text(temperature, msg));
	if (temperature < cold) {
		if (!g_pSysState->system.switches.button_buzzer_override)
			buzzer_feedback_value(100);
		if (!s->state.lowAlarm
				&& elapsed > g_pDevCfg->stTempAlertParams[id].maxSecondsCold) {
			sprintf(msg, "%s Below %s", SensorName[id],
					getFloatNumber2Text(cold, tmp));
			s->state.lowAlarm = true;
			goto alarm_error;
		}
		return;
	} else if (temperature > hot) {
		if (!g_pSysState->system.switches.button_buzzer_override)
			buzzer_feedback_value(100);
		if (!s->state.highAlarm
				&& elapsed > g_pDevCfg->stTempAlertParams[id].maxSecondsHot) {
			sprintf(msg, "%s Above %s", SensorName[id],
					getFloatNumber2Text(hot, tmp));
			s->state.highAlarm = true;
			goto alarm_error;
		}
		return;
	}

	return;

	alarm_error: s->state.alarm = true;
	state_alarm_on(msg);
	alarm_sms_sensor(id, elapsed);

	// Force the upload of data to the server in 15 seconds
	event_force_event_by_id(EVT_SUBSAMPLE_TEMP, 5);
	event_force_event_by_id(EVT_SAVE_SAMPLE_TEMP, 10);
	event_force_event_by_id(EVT_UPLOAD_SAMPLES, 15);
}

void alarm_monitor() {
	int8_t c;

	for (c = 0; c < SYSTEM_NUM_SENSORS; c++) {
		alarm_test_sensor(c);
	}
}
