/*
 * config.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_CONFIG_H_
#define TEMPSENSOR_V1_CONFIG_H_

#define EXTERN extern

#define USE_MININI

#include "stdint.h"
#include "common.h"
#include "time.h"

/**************************************************************************************************************************/
/* BEGIN FACTORY CONFIGURATION 																							  */
/**************************************************************************************************************************/

#ifndef _DEBUG
#define NEXLEAF_SMS_GATEWAY       "0000"

#define REPORT_PHONE_NUMBER 	   NEXLEAF_SMS_GATEWAY

#define NEXLEAF_DEFAULT_SERVER_IP "54.241.2.213"
#define NEXLEAF_DEFAULT_APN 	  "test.com"

// Path for getting the configuration from the server
// CONFIGURATION_URL_PATH/IMEI/1/
#define CONFIGURATION_URL_PATH "/coldtrace/configuration/ct5/v2/"
#define DATA_UPLOAD_URL_PATH "/coldtrace/uploads/multi/v4/"
#endif

// SMS alerts, it will send an SMS to the local testing number
#define ALERTS_SMS 1

// Threshold in percentage in which the device will enter deep hibernate mode
#define BATTERY_HIBERNATE_THRESHOLD 10
#define POWER_ENABLE_ALERT 1

// Thershold when the alarm will sound if there is a power cut (seconds)
#define THRESHOLD_TIME_POWER_OUT_ALARM 60*60*1

// Number of subsamples to capture per sample
#define NUM_SAMPLES_CAPTURE 10

//Sampling configuration
#ifndef _DEBUG
#define PERIOD_UNDEFINED		60
#define PERIOD_SAMPLING			5		//in minutes
#define PERIOD_UPLOAD			20		//in minutes
#define PERIOD_REBOOT 			24*60   //in minutes
#define PERIOD_TRANS_RESET		6*60   //in minutes
#define PERIOD_LCD_OFF			5
#define PERIOD_ALARMS_CHECK	    5
#define PERIOD_CONFIGURATION_FETCH 5
#define PERIOD_SMS_CHECK   	    16		//poll interval in minutes for sms msg TODO change back
#define PERIOD_NETWORK_CHECK	10
#define PERIOD_LCD_REFRESH		5
#define PERIOD_PULLTIME			12*60	// 12 hours
#define PERIOD_BATTERY_CHECK 	15
#define SAMPLE_COUNT			10
#endif

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

// Network signal quality values
#define NETWORK_DOWN_SS		14.0
#define NETWORK_UP_SS		NETWORK_DOWN_SS + 2 //2 points above the network down signal level
#define NETWORK_MAX_SS		31.0

#define NETWORK_ZERO 10.0

//Temperature cut off
#define TEMP_CUTOFF				-800		//-80 deg C

// 1 will disable the buzzer when there is an Alarm
// Buzzer will still work on button feedback
#define BUZZER_DISABLE 1

// Disable buttons sounds
#define BUZZER_DISABLE_FEEDBACK 1

/**************************************************************************************************************************/
/* END FACTORY CONFIGURATION 																							  */
/**************************************************************************************************************************/

//Number of sensors
#define SYSTEM_NUM_SENSORS		5
#define SYSTEM_NUM_SIM_CARDS 	2

/**************************************************************************************************************************/
/* NETWORK AND TIMEOUTS		  																						      */
/**************************************************************************************************************************/

#ifndef _DEBUG
#define MAIN_SLEEP_TIME 1000
#define MAIN_LCD_OFF_SLEEP_TIME 30000
#endif
#define MAIN_SLEEP_POWER_OUTAGE 5000

// Poll times trying to connect to the network.
// After autoband it could take up to 90 seconds for the bands trial and error.
// So we have to wait for the modem to be ready.

#ifndef _DEBUG
#define HTTP_COMMAND_ATTEMPTS 5
#define NETWORK_CONNECTION_ATTEMPTS 5
#define NETWORK_CONNECTION_DELAY 3000
#endif

#define MODEM_CHECK_RETRY 	3
#define NETWORK_PULLTIME_ATTEMPTS 3

/**************************************************************************************************************************/
/* DEVELOPMENT CONFIGURATION																							  */
/**************************************************************************************************************************/

#ifdef _DEBUG

#define NEXLEAF_SMS_GATEWAY       "0000"

#define REPORT_PHONE_NUMBER 	   NEXLEAF_SMS_GATEWAY

#define NEXLEAF_DEFAULT_SERVER_IP "0.0.0.0"
#define NEXLEAF_DEFAULT_APN 	  "test.com"

// Path for getting the configuration from the server
// CONFIGURATION_URL_PATH/IMEI/1/
#define CONFIGURATION_URL_PATH "/dummy/config"
#define DATA_UPLOAD_URL_PATH "/dummy/upload"

#define ALERTS_SMS 1

#define MAIN_SLEEP_TIME 100
#define MAIN_LCD_OFF_SLEEP_TIME 10000

#define PERIOD_UNDEFINED		60
#define PERIOD_SAMPLING			1		//in minutes
#define PERIOD_UPLOAD			10		//in minutes
#define PERIOD_REBOOT 			24*60   //in minutes
#define PERIOD_TRANS_RESET 		6*60   //in minutes
#define PERIOD_LCD_OFF			10
#define PERIOD_ALARMS_CHECK	    2
#define PERIOD_CONFIGURATION_FETCH 5
#define PERIOD_SMS_CHECK   	    3		//poll interval in minutes for sms msg TODO change back
#define PERIOD_NETWORK_CHECK	2
#define PERIOD_LCD_REFRESH		1
#define PERIOD_PULLTIME			2
#define PERIOD_BATTERY_CHECK 	2
#define SAMPLE_COUNT			5

#define PERIOD_SMS_TEST			30

#define HTTP_COMMAND_ATTEMPTS 2
#define NETWORK_CONNECTION_ATTEMPTS 100
#define NETWORK_CONNECTION_DELAY 2000
#endif

//Display contants
#define LCD_DISPLAY_LEN			32
#define LCD_INIT_PARAM_SIZE		9
#define LCD_LINE_LEN			16
#define DEF_IMEI  "IMEI_UNKNOWN"

/*****************************************************************************************************************/
/* Main structures for the application */
/*****************************************************************************************************************/
#define MODE_GSM 1
#define MODE_GPRS 2

typedef struct {
	char cfgSMSCenter[GW_MAX_LEN + 1]; // Service Message Center number
	char cfgPhoneNum[GW_MAX_LEN + 1];
	char cfgAPN[APN_MAX_LEN + 1];
	uint8_t iMaxMessages; // Max messages stored on sim card
	uint16_t iCfgMCC;
	uint16_t iCfgMNC;

	char simLastError[ERROR_MAX_LEN];
	uint16_t simErrorState;
	char simErrorToken;

	int8_t last_SMS_message;  // Last message sent with this SIM card;

	char networkMode;   // Connecting to network
	char networkStatus; // check NETWORK_MODE_1 array for status

	char SMSNotSupported;
	char simOperational; // The sim is in a functional state to send and receive messages

	int http_last_status_code;
} SIM_CARD_CONFIG;

// 255.255.255.255
#define MAX_IP_SIZE 3*4+3+1

// Careful with exceeding the size of the URL
#define MAX_URL_PATH 40

typedef struct {
	float threshCold;
	float threshHot;
	uint16_t maxSecondsCold;
	uint16_t maxSecondsHot;
} TEMP_ALERT_PARAM;

typedef struct {
	int minutesPower;
	int enablePowerAlert;
	int minutesBattThresh;
	int battThreshold;
} BATT_POWER_ALERT_PARAM;

typedef struct {
	uint16_t upload;
	uint16_t sampling;
	uint16_t systemReboot;
	uint16_t configurationFetch;
	uint16_t smsCheck;
	uint16_t networkCheck;
	uint16_t lcdOff;
	uint16_t alarmsCheck;
	uint16_t modemPullTime;
	uint16_t batteryCheck;
	uint16_t transmissionReset;
} INTERVAL_PARAM;

typedef union {
	struct {
		unsigned char system_log :1;
		unsigned char web_csv :1;
		unsigned char server_config :1;
		unsigned char modem_transactions :1;
		unsigned char sms_alerts :1;
		unsigned char sms_reports :1;
		unsigned char commmands :1;
		unsigned char bit8 :1;
	} logs;
	unsigned char status;
} LOGGING_COMPONENTS;

typedef struct {
#ifdef _DEBUG
	char cfgVersion[8];
#endif

	int8_t cfgSIM_force; //force sim
	int8_t cfgSIM_slot;
	int8_t cfgSelectedSIM_slot;

	int8_t cfgSyncId;

	int8_t cfgServerConfigReceived; // The server sent us a configuration package

	TEMP_ALERT_PARAM stTempAlertParams[SYSTEM_NUM_SENSORS];
	BATT_POWER_ALERT_PARAM stBattPowerAlertParam;
	INTERVAL_PARAM sIntervalsMins;

	char cfgIMEI[IMEI_MAX_LEN + 1];
	char cfgGatewayIP[MAX_IP_SIZE];
	char cfgGatewaySMS[GW_MAX_LEN + 1];
	char cfgConfig_URL[MAX_URL_PATH];
	char cfgUpload_URL[MAX_URL_PATH];

	// User that can get messages from the alarms
	char cfgReportSMS[GW_MAX_LEN + 1];

	int8_t cfgUploadMode;
	SIM_CARD_CONFIG SIM[SYSTEM_NUM_SIM_CARDS];
	struct tm lastSystemTime;

	LOGGING_COMPONENTS cfg;

} CONFIG_DEVICE;

typedef struct {
	uint8_t memoryInitialized;
	uint32_t numberRuns;
	uint32_t numberConfigurationRuns;
	uint8_t calibrationFinished;
	char firmwareVersion[17];
	uint16_t configStructureSize; // Size to check if there are changes on this structure

#ifdef _DEBUG_COUNT_BUFFERS
	// Stats to control buffer sizes
	uint16_t maxSamplebuffer;
	uint16_t maxRXBuffer;
	uint16_t maxTXBuffer;
#endif
#ifdef _DEBUG
	uint16_t stackLeft;
#endif

	uint16_t lastCommand; // Command that was last executed to control flow.
	char lastCommandTime[2 + 2 + 2 + 1 + 1 + 1]; //
} CONFIG_SYSTEM;

typedef struct {
	uint8_t modemErrors;
	uint8_t failsGPRS;
	uint8_t failsGSM;
} SIM_STATE;

#define STATUS_NO_ALARM 0

typedef union {
	struct {
		unsigned char alarm :1;	   // Alarm is on
		unsigned char lowAlarm :1;    // Temperature below minimum
		unsigned char highAlarm :1;   // Temperature above maximum
		unsigned char disconnected :1;   // Sensor not connected
		unsigned char bit5 :1;
		unsigned char bit6 :1;
		unsigned char bit7 :1;
		unsigned char bit8 :1;
	} state;
	unsigned char status;
} SENSOR_STATUS;

// Used to store the sensors data
#define TEMP_DATA_LEN		5

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
	SENSOR_STATUS state[SYSTEM_NUM_SENSORS];
	TEMPERATURE_SENSOR sensors[SYSTEM_NUM_SENSORS];
} TEMPERATURE;

typedef union {
	struct {
		unsigned char globalAlarm :1;
		unsigned char battery :1;
		unsigned char SD_card_failure :1;
		unsigned char poweroutage :1;
	} alarms;
	unsigned char status;
} SYSTEM_ALARMS;

typedef union {
	struct {
		unsigned char buzzer_disabled :1;
		unsigned char power_connected :1;
		unsigned char button_buzzer_override :1;
		unsigned char buzzer_sound :1;
		unsigned char http_enabled :1;
	} switches;
	unsigned char status;
} SYSTEM_SWITCHES;

//  Commands to ignore if there was a problem on last boot
typedef union {
	struct {
		unsigned char sms_process_messages :1; // Clear SMS
		unsigned char data_transmit :1;	// Delete old files since we are crashing the software (corruption?)
		unsigned char bit3 :1;
		unsigned char bit4 :1;
		unsigned char bit5 :1;
		unsigned char bit6 :1;
		unsigned char bit7 :1;
		unsigned char bit8 :1;
	} disable;
	unsigned char status;
} SAFEBOOT_STATUS;

typedef struct {
	char network_state[12];

	int network_presentation_mode;
	//NETWORK_STATUS_REGISTERED_HOME_NETWORK
	//NETWORK_STATUS_REGISTERED_ROAMING
	int network_status;

	uint8_t network_failures;
} NETWORK_SERVICE;

typedef struct {
	// Last alarm message
	char alarm_message[16];

	// Current battery level
	uint8_t battery_level;

	// Last time it was plugged
	time_t time_powerOutage;

	// SIM cards alarms and states
	SIM_STATE simState[MAX_SMS_NUM];

	// Current sim modem Signal level
	uint8_t signal_level;

	// Temperature of sensors and alarms
	TEMPERATURE temp;

	SYSTEM_SWITCHES system;
	SYSTEM_ALARMS state;

	// If transmission wasn't fully completed this will
	// contain the last line we didn't transmit
	uint32_t lastSeek;

	// GSM or GPRS
	int network_mode;
	NETWORK_SERVICE net_service[2];

	SAFEBOOT_STATUS safeboot;

	uint32_t buzzerFeedback; // Set to play sound on buzzer and activate buzzer
} SYSTEM_STATE;

typedef struct {
	int32_t dwLastSeek;
	float calibration[SYSTEM_NUM_SENSORS][2];
} CONFIG_CALIBRATION;

/*****************************************************************************************************************/
/* Configuration functions */
/*****************************************************************************************************************/

// Returns the current structure containing the info for the current SIM selected
SIM_CARD_CONFIG *config_getSIM();

// Returns current sim selected range [0..1]
uint8_t config_getSelectedSIM();

// Store the error of the SIM in memory to be displayed
void config_setSIMError(SIM_CARD_CONFIG *sim, char errorToken, uint16_t errorID,
		const char *error);

uint16_t config_getSIMError(int slot);
void config_reset_error(SIM_CARD_CONFIG *sim);
void config_display_config();
uint16_t config_getSimLastError(char *charToken);
int config_default_configuration();
int config_process_configuration();

#ifdef CONFIG_SAVE_IN_PROGRESS
void config_save_ini();
#endif

int config_parse_configuration(char *msg);

// Flags the sim as not working
void state_SIM_not_operational();

// Flags the sim as working again
void state_SIM_operational();

uint8_t config_is_SIM_configurable(int simSlot);

/*****************************************************************************************************************/
/* DIAGNOSE AND TESTING 			   																		     */
/* Check if there is a hang on next reset																		 */
/*****************************************************************************************************************/
#define COMMAND_EVENTS 0
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
#define COMMAND_FETCH_CONFIG 1700
#define COMMAND_SMS_PROCESS 1800
#define COMMAND_PARSE_CONFIG_ONLINE 1900
#define COMMAND_PARSE_CONFIG_ST1 1910
#define COMMAND_PARSE_CONFIG_ST2 1920
#define COMMAND_PARSE_CONFIG_ST3 1930

#define COMMAND_SET_NETWORK_SERVICE 2000
#define COMMAND_NETWORK_CONNECT 2100
#define COMMAND_SIM_ERROR 2200
#define COMMAND_UART_ERROR 2300
#define COMMAND_CHECK_NETWORK 2400
#define COMMAND_FIRST_INIT 2500
#define COMMAND_FAILOVER 2600
#define COMMAND_FAILOVER_HTTP_FAILED 2700
#define COMMAND_SWAP_SIM0 2800
#define COMMAND_SWAP_SIM1 2900
#define COMMAND_HTTP_ENABLE 3000
#define COMMAND_HTTP_DISABLE 3100
#define COMMAND_EVENT_CHECK_NETWORK 3200
#define COMMAND_EVENT_DEFERRED 3300
#define COMMAND_END 99

uint8_t state_isSimOperational();
void config_init();
void config_send_configuration(char *number);
void config_reconfigure();

void config_setLastCommand(uint16_t lastCmd);
void config_incLastCmd();

void config_update_system_time();

uint32_t config_get_boot_midnight_difference();
uint8_t check_address_empty(uint8_t mem);

// Setup mode in which we are at the moment
// Triggered by the Switch 3 button
extern int8_t g_iSystemSetup;

#endif /* TEMPSENSOR_V1_CONFIG_H_ */
