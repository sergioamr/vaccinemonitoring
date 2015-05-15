/*
 * defines.h
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 */

#ifndef DEFINES_H_
#define DEFINES_H_

#include "config.h"
#include "common.h"

// Alarm monitoring
#define TEMP_ALERT_OFF	 	 0
#define LOW_TEMP_ALARM_ON	 1
#define HIGH_TEMP_ALARM_ON	 2
#define TEMP_ALERT_CNF		 3

#define BATT_ALERT_OFF		 0
#define POWER_ALERT_OFF		 0
#define BATT_ALARM_ON		 1
#define POWER_ALARM_ON		 1

#define TEMP_ALARM_CLR(sid)  g_iAlarmStatus &= ~(3 << (sid << 1))
#define TEMP_ALARM_SET(sid,state) g_iAlarmStatus |= (state << (sid << 1))
#define TEMP_ALARM_GET(sid) ((g_iAlarmStatus & ((0x3) << (sid << 1)))) >> (sid << 1)

#define GSM 1
#define GPRS 2

#if MAX_NUM_SENSORS == 5
#define AGGREGATE_SIZE		256			//288
#define MAX_DISPLAY_ID		9
#else
#define AGGREGATE_SIZE		256
#define MAX_DISPLAY_ID		6
#endif

#endif /* DEFINES_H_ */
