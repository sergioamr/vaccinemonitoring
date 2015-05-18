/*
 * config.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define EXTERN extern

#include "stdint.h"

//Temperature cut off
#define TEMP_CUTOFF				-800		//-80 deg C
#define MODEM_CHECK_RETRY 	3

//I2C configuration
#define   I2C_TX_LEN			17
#define   I2C_RX_LEN			8
#define   SLAVE_ADDR_DISPLAY	0x38
#define   SLAVE_ADDR_BATTERY	0x55

//Battery configuration
#if BAT_VER == 1
#define   BATTERY_CAPACITY 		650			//650 mAh
#define   DESIGN_ENERGY			2405
#define   TERMINAL_VOLTAGE		3400
#define   TAPER_RATE			403
#else
#define   BATTERY_CAPACITY 		750			//750 mAh
#define   DESIGN_ENERGY			2850
#define   TERMINAL_VOLTAGE		3400
#define   TAPER_RATE			370
#define   TAPER_VOLT			4100
#endif

//Number of sensors
#define MAX_NUM_SENSORS			5

//Sampling configuration
#define SAMPLE_PERIOD			1		//in minutes
#define UPLOAD_PERIOD			10		//in minutes
#define SMS_RX_POLL_INTERVAL	5		//poll interval in minutes for sms msg
#define LCD_REFRESH_INTERVAL	1
#define MSG_REFRESH_INTERVAL	1
#define SAMPLE_COUNT			100

//Alert configuration
#define LOW_TEMP_THRESHOLD		2		//deg celsius
#define HIGH_TEMP_THRESHOLD		35		//deg celsius
#define ALARM_LOW_TEMP_PERIOD	30		//in minutes
#define ALARM_HIGH_TEMP_PERIOD	30		//in minutes
#define ALARM_POWER_PERIOD		30		//in minutes
#define ALARM_BATTERY_PERIOD	30		//in minutes
#define MIN_TEMP			  	-20
#define MAX_TEMP				55
#define MIN_CNF_TEMP_THRESHOLD  1
#define MAX_CNF_TEMP_THRESHOLD	1440

//Display contants
#define LCD_DISPLAY_LEN			32
#define LCD_INIT_PARAM_SIZE		9
#define LCD_LINE_LEN			16
#define DEF_IMEI  3000000000

//INFO memory segment address
#define INFOA_ADDR      		0x1980
#define INFOB_ADDR      		0x1900


EXTERN int g_iSamplePeriod;
EXTERN int g_iUploadPeriod;
EXTERN int8_t g_iAlarmLowTempPeriod;
EXTERN int8_t g_iAlarmHighTempPeriod;
EXTERN int8_t g_iAlarmPowerPeriod;
EXTERN int8_t g_iAlarmBatteryPeriod;

// Used to store the sensors data
#define TEMP_DATA_LEN		5

// Network signal quality values
#define NETWORK_DOWN_SS		14
#define NETWORK_UP_SS		NETWORK_DOWN_SS + 2 //2 points above the network down signal level
#define NETWORK_MAX_SS		31

#define NETWORK_ZERO 10

#endif /* CONFIG_H_ */
