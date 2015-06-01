/*
 * config.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_CONFIG_H_
#define TEMPSENSOR_V1_CONFIG_H_

#define EXTERN extern

#include "stdint.h"
#include "common.h"

#define MAX_NUM_CONTINOUS_SAMPLES 10

// Setup mode in which we are at the moment
// Triggered by the Switch 3 button
EXTERN int8_t g_iSystemSetup;

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
char    				cfgGateway[GW_MAX_LEN + 1];
char    				cfgSMSCenter[NUM_SIM_CARDS][GW_MAX_LEN + 1]; // Service Message Center number
char    				cfgAPN[NUM_SIM_CARDS][APN_MAX_LEN + 1];
uint8_t					iMaxMessages[NUM_SIM_CARDS];
uint16_t 				iCfgMCC[NUM_SIM_CARDS];
uint16_t 				iCfgMNC[NUM_SIM_CARDS];
} CONFIG_INFOA;

typedef struct __attribute__((__packed__))  {
	uint8_t memoryInitialized;
	uint32_t numberRuns;
	uint32_t numberConfigurationRuns;
	uint8_t calibrationFinished;
	char firmwareVersion[17];
	uint16_t configStructureSize; // Size to check if there are changes on this structure

	// Stats to control buffer sizes
	uint16_t maxSamplebuffer;
	uint16_t maxATResponse;
	uint16_t maxRXBuffer;
	uint16_t maxTXBuffer;

	uint16_t lastCommand; // Command that was last executed to control flow.
	char lastCommandTime[2+2+2+1+1+1]; //
} CONFIG_SYSTEM;

typedef struct {
int32_t	   dwLastSeek;
double	   calibration[MAX_NUM_SENSORS][2];
} CONFIG_INFOB;

/*****************************************************************************************************************/
/* DIAGNOSE AND TESTING 			   																		     */
/* Check if there is a hang on next reset																		 */
/*****************************************************************************************************************/
#define COMMAND_BOOT 100
#define COMMAND_TEMPERATURE_SAMPLE 200
#define COMMAND_GPRS 300
#define COMMAND_MODEMINIT 400
#define COMMAND_FATINIT 500
#define COMMAND_FATSAVE 600
#define COMMAND_SMS_SEND 700
#define COMMAND_LCDINIT 800
#define COMMAND_CALIBRATION 1000
#define COMMAND_POST 1100
#define COMMAND_TEMPERATURE 1200

#define COMMAND_END 99

extern void config_init();
extern void config_setLastCommand(uint16_t lastCmd);
extern void config_incLastCmd();

#endif /* TEMPSENSOR_V1_CONFIG_H_ */
