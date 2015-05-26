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
#include "common.h"

//Temperature cut off
#define TEMP_CUTOFF				-800		//-80 deg C
#define MODEM_CHECK_RETRY 	3
#define MAX_TIME_ATTEMPTS 3

//I2C configuration
#define   I2C_TX_LEN			32
#define   I2C_RX_LEN			32
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
#define SMS_RX_POLL_INTERVAL	5		//poll interval in minutes for sms msg TODO change back
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
#define DEF_IMEI  "IMEI_UNKNOWN"

//INFO memory segment address
//#define INFOA_ADDR      		0x1980
//#define INFOB_ADDR      		0x1900

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

#define NUM_SIM_CARDS 2

typedef struct __attribute__((__packed__))  {
int8_t 				    cfgUploadMode;
int8_t					cfgSIMSlot;
TEMP_ALERT_PARAM		stTempAlertParams[MAX_NUM_SENSORS];
BATT_POWER_ALERT_PARAM	stBattPowerAlertParam;
char    				cfgIMEI[IMEI_MAX_LEN + 1];
char    				cfgSMSCenter[NUM_SIM_CARDS][GW_MAX_LEN + 1]; // Service Message Center number
char    				cfgAPN[NUM_SIM_CARDS][APN_MAX_LEN + 1];
char					cfgMCC[NUM_SIM_CARDS][MCC_MAX_LEN + 1];
char					cfgMNC[NUM_SIM_CARDS][MNC_MAX_LEN + 1];
} CONFIG_INFOA;

typedef struct __attribute__((__packed__))  {
	uint8_t memoryInitialized;
	uint32_t numberRuns;
	uint32_t numberConfigurationRuns;
	uint8_t calibrationFinished;
	char firmwareVersion[64];
	uint16_t configStructureSize; // Size to check if there are changes on this structure
} CONFIG_SYSTEM;

typedef struct {
int32_t	   dwLastSeek;
double	   calibration[MAX_NUM_SENSORS][2];
} CONFIG_INFOB;

extern void config_Init();

#endif /* CONFIG_H_ */
