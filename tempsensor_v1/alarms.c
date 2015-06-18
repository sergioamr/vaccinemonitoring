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
void alarm_sensor_sms(uint8_t sensorId, int elapsed) {
	char msg[MAX_SMS_SIZE];
	sprintf(msg, "Alert Sensor %s at %s degC for %d minutes", SensorName[sensorId],
			temperature_getString(sensorId), elapsed/60 );
	strcat(msg, " Take ACTION immediately.");
	sms_send_message(msg);
}

SENSOR_STATUS *getAlarmsSensor(int id) {
	USE_TEMPERATURE
	return &tem->state[id];
}

void alarm_test_sensor(int id) {
	char msg[32];
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
			log_appendf("Alarm %d recovered ", id);
		}
		return;
	}

	if (tem->alarm_time == 0)
		tem->alarm_time = events_getTick();
	else
		elapsed = events_getTick() - tem->alarm_time;

	lcd_printf(LINEC, "Sensor \"%s\" %s", SensorName[id],
			getFloatNumber2Text(temperature, msg));
	if (temperature < cold) {
		if (elapsed > g_pDevCfg->stTempAlertParams[id].maxTimeCold) {
			sprintf(msg, "%s too cold", SensorName[id]);
			s->alarms.lowAlarm = true;
			goto alarm_error;
		}

		lcd_printf(LINEE, "Below limit %s", getFloatNumber2Text(hot, msg));
		return;
	} else if (temperature > hot) {
		if (elapsed > g_pDevCfg->stTempAlertParams[id].maxTimeHot) {
			sprintf(msg, "%s too hot", SensorName[id]);
			s->alarms.highAlarm = true;
			goto alarm_error;
		}

		lcd_printf(LINEE, "Above limit %s", getFloatNumber2Text(hot, msg));
		return;
	}

	alarm_error: s->alarms.alarm = true;
	state_alarm_on(msg);
	alarm_sensor_sms(id, elapsed);
}

void alarm_monitor() {
	int8_t c;

	for (c = 0; c < MAX_NUM_SENSORS; c++) {
		alarm_test_sensor(c);
	}

}

/*
 void validatealarmthreshold() {

 int8_t iCnt = 0;

 for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
 //validate low temp threshold
 if ((g_pDevCfg->stTempAlertParams[iCnt].threshCold < MIN_TEMP)
 || (g_pDevCfg->stTempAlertParams[iCnt].threshCold > MAX_TEMP)) {
 g_pDevCfg->stTempAlertParams[iCnt].threshCold =
 LOW_TEMP_THRESHOLD;
 }

 if ((g_pDevCfg->stTempAlertParams[iCnt].minCold < MIN_CNF_TEMP_THRESHOLD)
 || (g_pDevCfg->stTempAlertParams[iCnt].minCold
 > MAX_CNF_TEMP_THRESHOLD)) {
 g_pDevCfg->stTempAlertParams[iCnt].minCold =
 ALARM_LOW_TEMP_PERIOD;
 }

 //validate high temp threshold
 if ((g_pDevCfg->stTempAlertParams[iCnt].threshHot < MIN_TEMP)
 || (g_pDevCfg->stTempAlertParams[iCnt].threshHot > MAX_TEMP)) {
 g_pDevCfg->stTempAlertParams[iCnt].threshHot =
 HIGH_TEMP_THRESHOLD;
 }
 if ((g_pDevCfg->stTempAlertParams[iCnt].minHot < MIN_CNF_TEMP_THRESHOLD)
 || (g_pDevCfg->stTempAlertParams[iCnt].minHot
 > MAX_CNF_TEMP_THRESHOLD)) {
 g_pDevCfg->stTempAlertParams[iCnt].minHot =
 ALARM_HIGH_TEMP_PERIOD;
 }

 }

 }

 int8_t iCnt = 0;
 char msg[SMS_MAX_SIZE];

 //iterate through sensors
 for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
 //get sensor value
 //temp = &Temperature[iCnt];

 //check sensor was plugged out
 if (Temperature[iCnt][1] == '-') {
 TEMP_ALARM_CLR(iCnt);
 g_iAlarmCnfCnt[iCnt] = 0;
 continue;
 }

 iTemp = strtod(&Temperature[iCnt][0], NULL);
 //iTemp = strtod("24.5",NULL);
 //check for low temp threshold
 if (iTemp < g_pDevCfg->stTempAlertParams[iCnt].threshCold) {
 //check if alarm is set for low temp
 if (TEMP_ALARM_GET(iCnt) != LOW_TEMP_ALARM_ON) {
 //set it off incase it was earlier confirmed
 TEMP_ALARM_CLR(iCnt);
 //set alarm to low temp
 TEMP_ALARM_SET(iCnt, LOW_TEMP_ALARM_ON);
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;
 } else if (TEMP_ALARM_GET(iCnt) == LOW_TEMP_ALARM_ON) {
 //low temp alarm is already set, increment the counter
 g_iAlarmCnfCnt[iCnt]++;
 if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
 >= g_pDevCfg->stTempAlertParams[iCnt].minCold) {
 TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
 #ifndef ALERT_UPLOAD_DISABLED
 if(!(g_iStatus & BACKLOG_UPLOAD_ON))
 {
 g_iStatus |= ALERT_UPLOAD_ON;//indicate to upload sampled data if backlog is not progress
 }
 #endif
 //set buzzer if not set
 if (!(g_iStatus & BUZZER_ON)) {
 //P3OUT |= BIT4;
 g_iStatus |= BUZZER_ON;
 }
 #ifdef SMS_ALERT
 //send sms if not send already

 #endif

 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;

 }
 }
 } else if (iTemp > g_pDevCfg->stTempAlertParams[iCnt].threshHot) {
 //check if alarm is set for high temp
 if (TEMP_ALARM_GET(iCnt) != HIGH_TEMP_ALARM_ON) {
 //set it off incase it was earlier confirmed
 TEMP_ALARM_CLR(iCnt);
 //set alarm to high temp
 TEMP_ALARM_SET(iCnt, HIGH_TEMP_ALARM_ON);
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;
 } else if (TEMP_ALARM_GET(iCnt) == HIGH_TEMP_ALARM_ON) {
 //high temp alarm is already set, increment the counter
 g_iAlarmCnfCnt[iCnt]++;
 if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
 >= g_pDevCfg->stTempAlertParams[iCnt].minHot) {
 TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
 #ifndef ALERT_UPLOAD_DISABLED
 if(!(g_iStatus & BACKLOG_UPLOAD_ON))
 {
 g_iStatus |= ALERT_UPLOAD_ON;//indicate to upload sampled data if backlog is not progress
 }
 #endif
 //set buzzer if not set
 if (!(g_iStatus & BUZZER_ON)) {
 //P3OUT |= BIT4;
 g_iStatus |= BUZZER_ON;
 }
 #ifdef SMS_ALERT
 strcpy(msg, "Alert Sensor ");
 strcat(msg, SensorName[iCnt]);
 strcat(msg,": Temp too HIGH for ");
 strcat(msg,itoa_pad(g_pDevCfg->stTempAlertParams[iCnt].minHot));
 strcat(msg," minutes. Current Temp is ");
 strcat(msg,Temperature[iCnt]);
 //strcat(msg,"�C. Take ACTION immediately."); //superscript causes ERROR on sending SMS
 strcat(msg,"degC. Take ACTION immediately.");
 sms_send_message(msg);
 #endif
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;

 }
 }

 } else {
 //check if alarm is set
 if (TEMP_ALARM_GET(iCnt) != TEMP_ALERT_OFF) {
 //reset the alarm
 TEMP_ALARM_CLR(iCnt);
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;
 }
 }
 }

 //check for battery alert
 iCnt = MAX_NUM_SENSORS;
 if (g_pSysState->battery_level < g_pDevCfg->stBattPowerAlertParam.battThreshold) {
 //check if battery alarm is set
 if (TEMP_ALARM_GET(iCnt) != BATT_ALARM_ON) {
 TEMP_ALARM_CLR(iCnt);//reset the alarm in case it was earlier confirmed
 //set battery alarm
 TEMP_ALARM_SET(iCnt, BATT_ALARM_ON);
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;
 } else if (TEMP_ALARM_GET(iCnt) == BATT_ALARM_ON) {
 //power alarm is already set, increment the counter
 g_iAlarmCnfCnt[iCnt]++;
 if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
 >= g_pDevCfg->stBattPowerAlertParam.minutesBattThresh) {
 TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
 //set buzzer if not set
 if (!(g_iStatus & BUZZER_ON)) {
 //P3OUT |= BIT4;
 g_iStatus |= BUZZER_ON;
 }
 #ifdef SMS_ALERT
 //send sms LOW Battery: ColdTrace has now 15% battery left. Charge your device immediately.
 strcpy(msg, "LOW Battery: ColdTrace has now ");
 strcat(msg,itoa_pad(batt_getlevel()));
 strcat(msg, "battery left. Charge your device immediately.");
 sms_send_message(msg);
 #endif

 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;

 }
 }
 } else {
 //reset battery alarm
 TEMP_ALARM_SET(iCnt, BATT_ALERT_OFF);
 g_iAlarmCnfCnt[iCnt] = 0;
 }

 //check for power alert enable
 if (g_pDevCfg->stBattPowerAlertParam.enablePowerAlert) {
 iCnt = MAX_NUM_SENSORS + 1;
 if (P4IN & BIT4) {
 //check if power alarm is set
 if (TEMP_ALARM_GET(iCnt) != POWER_ALARM_ON) {
 TEMP_ALARM_CLR(iCnt);//reset the alarm in case it was earlier confirmed
 //set power alarm
 TEMP_ALARM_SET(iCnt, POWER_ALARM_ON);
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;
 } else if (TEMP_ALARM_GET(iCnt) == POWER_ALARM_ON) {
 //power alarm is already set, increment the counter
 g_iAlarmCnfCnt[iCnt]++;
 if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
 >= g_pDevCfg->stBattPowerAlertParam.minutesPower) {
 TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
 //set buzzer if not set
 if (!(g_iStatus & BUZZER_ON)) {
 //P3OUT |= BIT4;
 g_iStatus |= BUZZER_ON;
 }
 #ifdef SMS_ALERT
 //send sms Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.
 strcpy(msg, "Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.");
 sms_send_message(msg);
 #endif
 //initialize confirmation counter
 g_iAlarmCnfCnt[iCnt] = 0;

 }
 }
 } else {
 //reset power alarm
 TEMP_ALARM_SET(iCnt, POWER_ALERT_OFF);
 g_iAlarmCnfCnt[iCnt] = 0;
 }
 }

 //disable buzzer if it was ON and all alerts are gone
 if ((g_iStatus & BUZZER_ON) && (g_iAlarmStatus == 0)) {
 g_iStatus &= ~BUZZER_ON;
 }

 */
