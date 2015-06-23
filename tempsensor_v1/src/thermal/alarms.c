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
	char msg[MAX_SMS_SIZE];
	if (!g_pDevCfg->cfgSMS_Alerts)
		return;

	strcpy(msg, "LOW Battery: ColdTrace has now ");
	strcat(msg, itoa_pad(batt_getlevel()));
	strcat(msg, "battery left. Charge your device immediately.");
	sms_send_message_number(REPORT_PHONE_NUMBER, msg);
}

void alarm_sms_sensor(uint8_t sensorId, int elapsed) {
	char msg[MAX_SMS_SIZE];
	if (!g_pDevCfg->cfgSMS_Alerts)
		return;

	sprintf(msg, "Alert Sensor %s at %s degC for %d minutes",
			SensorName[sensorId], temperature_getString(sensorId),
			elapsed / 60);
	strcat(msg, " Take ACTION immediately.");
	sms_send_message_number(REPORT_PHONE_NUMBER, msg);
}

void alarm_sms_power_outage() {
	if (!g_pDevCfg->cfgSMS_Alerts)
			return;

	sms_send_message_number(REPORT_PHONE_NUMBER, "Power Outage: ATTENTION! \r\n"
			"Power is out in your clinic. \r\n"
			"Monitor the fridge temperature closely.");
}

SENSOR_STATUS *getAlarmsSensor(int id) {
	USE_TEMPERATURE
	return &tem->state[id];
}

void alarm_test_sensor(int id) {
	char msg[32];
	char tmp[6];
	USE_TEMPERATURE
	SENSOR_STATUS *s = getAlarmsSensor(id);
	TEMPERATURE_SENSOR *sensor = &tem->sensors[id];
	time_t elapsed = 0;

	float cold = g_pDevCfg->stTempAlertParams[id].threshCold;
	float hot = g_pDevCfg->stTempAlertParams[id].threshHot;

	float temperature = sensor->fTemperature;
	if (temperature == 0) {
		if (s->alarms.disconnected == false) {
			s->alarms.disconnected = true;
			sprintf(msg, "%s disconnected", SensorName[id]);
			goto alarm_error;
		};
		return;
	}

	if (temperature > cold && temperature < hot) {
		if (s->alarms.alarm) {
			s->status = STATUS_NO_ALARM;
			tem->alarm_time = 0;
			g_pSysState->system.alarms.button_buzzer_override = false;
			log_appendf("Alarm %d recovered ", id);
		}
		return;
	}

	if (s->alarms.alarm)
		return;

	if (tem->alarm_time == 0)
		tem->alarm_time = events_getTick();
	else
		elapsed = events_getTick() - tem->alarm_time;

	//lcd_printf(LINEC, "Sensor \"%s\" %s", SensorName[id], getFloatNumber2Text(temperature, msg));
	if (temperature < cold) {
		if (!s->alarms.lowAlarm
				&& elapsed > g_pDevCfg->stTempAlertParams[id].maxSecondsCold) {
			sprintf(msg, "%s Below limit %s ", SensorName[id],
					getFloatNumber2Text(cold, tmp));
			s->alarms.lowAlarm = true;
			goto alarm_error;
		}
		return;
	} else if (temperature > hot) {
		if (!s->alarms.highAlarm
				&& elapsed > g_pDevCfg->stTempAlertParams[id].maxSecondsHot) {
			sprintf(msg, "%s Above limit %s ", SensorName[id],
					getFloatNumber2Text(hot, tmp));
			s->alarms.highAlarm = true;
			goto alarm_error;
		}
		return;
	}

	alarm_error: s->alarms.alarm = true;
	state_alarm_on(msg);
	alarm_sms_sensor(id, elapsed);
}

void alarm_monitor() {
	int8_t c;

	for (c = 0; c < SYSTEM_NUM_SENSORS; c++) {
		alarm_test_sensor(c);
	}
}