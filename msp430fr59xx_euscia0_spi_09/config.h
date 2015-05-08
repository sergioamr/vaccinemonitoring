/*
 * config.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#ifdef CONFIG_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#include "stdint.h"

//I2C configuration
#define   I2C_TX_LEN			17
#define   I2C_RX_LEN			8
#define   SLAVE_ADDR_DISPLAY	0x38
#define   SLAVE_ADDR_BATTERY	0x55

//Battery configuration
#define   BATTERY_CAPACITY 		650			//650 mAh

//Sampling configuration
#define SAMPLE_PERIOD			1		//in minutes
#define UPLOAD_PERIOD			10		//in minutes
#define ALARM_LOW_TEMP_PERIOD	30		//in minutes
#define ALARM_HIGH_TEMP_PERIOD	30		//in minutes
#define ALARM_POWER_PERIOD		30		//in minutes
#define ALARM_BATTERY_PERIOD	30		//in minutes



EXTERN int g_iSamplePeriod;
EXTERN int g_iUploadPeriod;
EXTERN int8_t g_iAlarmLowTempPeriod;
EXTERN int8_t g_iAlarmHighTempPeriod;
EXTERN int8_t g_iAlarmPowerPeriod;
EXTERN int8_t g_iAlarmBatteryPeriod;
#endif /* CONFIG_H_ */
