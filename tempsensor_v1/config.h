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
#include "time.h"

/**************************************************************************************************************************/
/* BEGIN FACTORY CONFIGURATION 																							  */
/**************************************************************************************************************************/

#define NEXLEAF_SMS_GATEWAY       "00447482787262"
#define NEXLEAF_DEFAULT_SERVER_IP "54.241.2.213"
#define NEXLEAF_DEFAULT_APN 	  "giffgaff.com"

#define NETWORK_ATTEMPTS_BEFORE_SWAP_SIM 3

#define LOCAL_TESTING_NUMBER "07977345678"

// Threshold in percentage in which the device will enter deep hibernate mode
#define BATTERY_HIBERNATE_THRESHOLD 10
#define POWER_ENABLE_ALERT 1

// Thershold when the alarm will sound if there is a power cut (seconds)
#define THRESHOLD_TIME_POWER_OUT_ALARM 60*60*1

// Path for getting the configuration from the server
// CONFIGURATION_URL_PATH/IMEI/1/
#define CONFIGURATION_URL_PATH "/coldtrace/uploads/multi/v3"

#ifdef _DEBUG
#define NUM_SAMPLES_CAPTURE 10
#else
#define NUM_SAMPLES_CAPTURE 10
#endif

/**************************************************************************************************************************/
/* END FACTORY CONFIGURATION 																							  */
/**************************************************************************************************************************/

#ifdef _DEBUG
#define MAIN_SLEEP_TIME 1000
#define MAIN_LCD_OFF_SLEEP_TIME 10000
#else
#define MAIN_SLEEP_TIME 1000
#define MAIN_LCD_OFF_SLEEP_TIME 30000
#endif

// Poll times trying to connect to the network.
// After autoband it could take up to 90 seconds for the bands trial and error.
// So we have to wait for the modem to be ready.

#ifdef _DEBUG
#define HTTP_COMMAND_ATTEMPTS 10
#define NETWORK_CONNECTION_ATTEMPTS 20
#define NETWORK_CONNECTION_DELAY 1000
#else
#define HTTP_COMMAND_ATTEMPTS 40
#define NETWORK_CONNECTION_ATTEMPTS 10
#define NETWORK_CONNECTION_DELAY 5000
#endif

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
#define UPLOAD_PERIOD			2		//in minutes
#define REBOOT_PERIOD 			24*60   //in minutes
#define CONFIGURATION_FETCH_PERIOD 30
#define SMS_RX_POLL_INTERVAL	3		//poll interval in minutes for sms msg TODO change back
#define LCD_REFRESH_INTERVAL	1
#define MSG_REFRESH_INTERVAL	5
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

// Used to store the sensors data
#define TEMP_DATA_LEN		5

// Network signal quality values
#define NETWORK_DOWN_SS		14.0
#define NETWORK_UP_SS		NETWORK_DOWN_SS + 2 //2 points above the network down signal level
#define NETWORK_MAX_SS		31.0

#define NETWORK_ZERO 10.0

#define NUM_SIM_CARDS 2

typedef struct {
	char cfgSMSCenter[GW_MAX_LEN + 1]; // Service Message Center number
	char cfgPhoneNum[GW_MAX_LEN + 1];
	char cfgAPN[APN_MAX_LEN + 1];
	uint8_t iMaxMessages; // Max messages stored on sim card
	uint16_t iCfgMCC;
	uint16_t iCfgMNC;
	int8_t cfgUploadMode;

	char simLastError[ERROR_MAX_LEN];
	uint16_t simErrorState;
	char simErrorToken;

	char networkMode;   // Connecting to network
	char networkStatus; // check NETWORK_MODE_1 array for status
	char simOperational; // The sim is in a functional state to send and receive messages
} SIM_CARD_CONFIG;

// 255.255.255.255
#define MAX_IP_SIZE 3*4+3+1

typedef struct {
	float threshCold;
	float threshHot;
	uint32_t maxTimeCold;
	uint32_t maxTimeHot;
} TEMP_ALERT_PARAM;

typedef struct {
	int minutesPower;
	int enablePowerAlert;
	int minutesBattThresh;
	int battThreshold;
} BATT_POWER_ALERT_PARAM;

typedef struct {
	uint16_t uploadInterval;
	uint16_t loggingInterval;
	uint16_t reboot;
	uint16_t configuration_fetch;
} INTERVAL_PARAM;

typedef struct {
	int8_t cfgSIM_slot;
	int8_t cfgSelectedSIM_slot;

	int8_t cfgSyncId;

	int8_t cfgServerConfigReceived; // The server sent us a configuration package

	TEMP_ALERT_PARAM stTempAlertParams[MAX_NUM_SENSORS];
	BATT_POWER_ALERT_PARAM stBattPowerAlertParam;
	INTERVAL_PARAM stIntervalParam;

	char cfgIMEI[IMEI_MAX_LEN + 1];
	char cfgGatewayIP[MAX_IP_SIZE];
	char cfgGatewaySMS[GW_MAX_LEN + 1];

	SIM_CARD_CONFIG SIM[NUM_SIM_CARDS];
	struct tm lastSystemTime;

} CONFIG_DEVICE;

typedef struct  {
	uint8_t memoryInitialized;
	uint32_t numberRuns;
	uint32_t numberConfigurationRuns;
	uint32_t lastSeek;
	uint8_t calibrationFinished;
	char firmwareVersion[17];
	uint16_t configStructureSize; // Size to check if there are changes on this structure

	// Stats to control buffer sizes
	uint16_t maxSamplebuffer;
	uint16_t maxRXBuffer;
	uint16_t maxTXBuffer;

	uint16_t lastCommand; // Command that was last executed to control flow.
	char lastCommandTime[2 + 2 + 2 + 1 + 1 + 1]; //
} CONFIG_SYSTEM;

typedef struct {
	uint8_t modemErrors;
	uint8_t failsGPRS;
	uint8_t failsGSM;
} SIM_STATE;

#define STATUS_NO_ALARM 0

typedef union
{
	struct {
		unsigned char alarm : 1;	   // Alarm is on
		unsigned char lowAlarm : 1;    // Temperature below minimum
		unsigned char highAlarm : 1;   // Temperature above maximum
		unsigned char disconnected : 1;// Sensor not connected
		unsigned char bit5 : 1;
		unsigned char bit6 : 1;
		unsigned char bit7 : 1;
		unsigned char bit8 : 1;
	} alarms;
  unsigned char status;
} SENSOR_STATUS;

typedef struct {
	char name[2];
	volatile int32_t iADC;
	float fTemperature;
	uint16_t iSamplesRead;
	char temperature[TEMP_DATA_LEN + 1];
} TEMPERATURE_SENSOR;

typedef struct {
	// Raw voltage values
	uint16_t iCapturing;
	uint16_t iSamplesRequired;
	uint16_t iSamplesRead;

	time_t alarm_time;	// When was the alarm triggered
	SENSOR_STATUS state[MAX_NUM_SENSORS];
	TEMPERATURE_SENSOR sensors[MAX_NUM_SENSORS];
} TEMPERATURE;

typedef union
{
	struct {
		unsigned char globalAlarm : 1;
		unsigned char SD_cardFailure : 1;
		unsigned char buzzer : 1;
		unsigned char power : 1;
		unsigned char bit5 : 1;
		unsigned char battery : 1;
		unsigned char bit7 : 1;
		unsigned char bit8 : 1;
	} alarms;
  unsigned char status;
} SYSTEM_STATUS;

typedef struct {
	char alarm_message[32];
	uint8_t battery_level;
	time_t time_powerOutage;
	uint8_t buzzerFeedback;
	SIM_STATE simState[MAX_SMS_NUM];

	uint8_t signal_level;
	char network_state[32];

	TEMPERATURE temp;
	SYSTEM_STATUS system;

	int network_mode;

	//NETWORK_STATUS_REGISTERED_HOME_NETWORK
	//NETWORK_STATUS_REGISTERED_ROAMING
	int network_status;

	uint8_t network_failures;

} SYSTEM_STATE;

typedef struct {
	int32_t dwLastSeek;
	double calibration[MAX_NUM_SENSORS][2];
} CONFIG_CALIBRATION;

// Returns the current structure containing the info for the current SIM selected
SIM_CARD_CONFIG *config_getSIM();

// Returns current sim selected range [0..1]
uint8_t config_getSelectedSIM();

// Store the error of the SIM in memory to be displayed
void config_setSIMError(SIM_CARD_CONFIG *sim, char errorToken,	uint16_t errorID, const char *error);

extern uint16_t config_getSIMError(int slot);
extern void config_reset_error(SIM_CARD_CONFIG *sim);
extern uint16_t config_getSimLastError(char *charToken);

int config_process_configuration();
int config_parse_configuration(char *msg);

// Flags the sim as not working
void config_SIM_not_operational();

// Flags the sim as working again
void config_SIM_operational();

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
#define COMMAND_MAIN_LCD_DISPLAY 1300
#define COMMAND_BOOT_MIDNIGHT 1400
#define COMMAND_MONITOR_ALARM 1500
#define COMMAND_NETWORK_SIGNAL_MONITOR 1600
#define COMMAND_HTTP_DATA_TRANSFER 1700
#define COMMAND_SMS_PROCESS 1800

#define COMMAND_END 99

void config_init();
void config_send_configuration(char *number);
void config_reconfigure();
void config_save_command(char *string);
void config_setLastCommand(uint16_t lastCmd);
void config_incLastCmd();
void config_update_system_time();

uint32_t config_get_boot_midnight_difference();
uint8_t check_address_empty(uint8_t mem);

#endif /* TEMPSENSOR_V1_CONFIG_H_ */
