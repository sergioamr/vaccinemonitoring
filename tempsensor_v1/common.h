/*
 * common.h
 *
 *  Created on: Mar 19, 2015
 *      Author: rajeevnx
 */

#ifndef COMMON_H_
#define COMMON_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Defines for functionality configuration
 */
//#define POWER_SAVING_ENABLED // TODO Code originally commented
#define ENABLE_SIM_SLOT		//needed to set on new board, comment it for old board
#define SEQUENCE
//#define FILE_TEST
//#define SAMPLE_POST
#define NO_CODE_SIZE_LIMIT
//#define BATTERY_DISABLED
//#define I2C_DISABLED

#define NOTFROMFILE
#define BUZZER_DISABLED
#define ALERT_UPLOAD_DISABLED
//#define CALIBRATION			//set this flag whenever the device has to undergo calibration
#define MIN 14

/*
 *  Alarm monitoring
 */

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

/*
 * Communication
 */

#define GSM 1
#define GPRS 2

/*
 * Timeouts for modem transactions
 *
 * TODO Modify delay times with the documentation for the Telit modem
 * see section 3.2.4 Command Response Time-Out
 * https://www.sparkfun.com/datasheets/Cellular%20Modules/AT_Commands_Reference_Guide_r0.pdf
 *
 */

#ifdef _DEBUG
#define MODEM_TX_DELAY1		1000
#define MODEM_TX_DELAY2		5000
#else
#define MODEM_TX_DELAY1		1000
#define MODEM_TX_DELAY2		5000
#endif

/*
 * Defines for data sizes
 */

//len constants
#define IMEI_MAX_LEN		 15
#define GW_MAX_LEN			 12
#define APN_MAX_LEN     	 20
#define UPLOAD_MODE_LEN 	 4
#define MAX_SMS_NUM			 4
#define SMS_NUM_LEN			 12
#define SMS_READ_MAX_MSG_IDX 25		//ZZZZ to change date based on CPMS
#define SAMPLE_LEN			 470
#define RX_EXTENDED_LEN 	 300	//to retreive a complete cfg message via SMS/HTTP message by reusing POST buffer
#define CFG_SIZE			 168	//can go upto 200
#define SAMPLE_SIZE			 460

#define SMS_ENCODED_LEN		 154	//ZZZZ SMS_ENCODED_LEN + ENCODED_TEMP_LEN should less than aggregate_var size - RX buff size
#define ENCODED_TEMP_LEN	 3
#define MSG_RESPONSE_LEN	 100
#define PHONE_NUM_LEN        14
#define SMS_CMD_LEN 		 36

#define SMS_HB_MSG_TYPE  	 "10,"
#define SMS_DATA_MSG_TYPE	 "11,"

#define SMS_DEFAULT_CENTER  "00447802002606" // Disabled
#define SMS_NEXLEAF_GATEWAY "00447751035864"

/*
 * Constants for different uses
 */

#if MAX_NUM_SENSORS == 5
#define AGGREGATE_SIZE		256			//288
#define MAX_DISPLAY_ID		9
#else
#define AGGREGATE_SIZE		256
#define MAX_DISPLAY_ID		6
#endif


//iStatus contants
#define MODEM_POWERED_ON 	0x0001
#define ALERT_UPLOAD_ON		0x0002
#define LOG_TIME_STAMP		0x0004
#define SPLIT_TIME_STAMP	0x0008
#define ENABLE_SECOND_SLOT	0x0010
#define RESET_ALERT  		0x0020
#define BUZZER_ON	  		0x0040
#define TEST_FLAG			0x0080
#define BACKLOG_UPLOAD_ON	0x0100
#define NETWORK_DOWN		0x0200

#define LOW_RANGE_MIN 		0
#define LOW_RANGE_MAX 		10

#define MED_RANGE_MIN 		11
#define MED_RANGE_MAX 		20

#define HIGH_RANGE_MIN 		21
#define HIGH_RANGE_MAX 		31

typedef struct {
	int mincold;
	double threshcold;
	int minhot;
	double threshhot;
} TEMP_ALERT_PARAM;

typedef struct {
	int minutespower;
	int enablepoweralert;
	int minutesbathresh;
	int battthreshold;
} BATT_POWER_ALERT_PARAM;

#define NUM_SIM_CARDS 2

typedef struct {
int8_t 				    cfgUploadMode;
int8_t					cfgSIMSlot;
TEMP_ALERT_PARAM		stTempAlertParams[MAX_NUM_SENSORS];
BATT_POWER_ALERT_PARAM	stBattPowerAlertParam;
char    				cfgIMEI[IMEI_MAX_LEN + 1];
char    				cfgSMSCenter[NUM_SIM_CARDS][GW_MAX_LEN + 1]; // Service Message Center number
char    				cfgAPN[NUM_SIM_CARDS][APN_MAX_LEN + 1];
} CONFIG_INFOA;

typedef struct {
int32_t	   dwLastSeek;
double	   calibration[MAX_NUM_SENSORS][2];
} CONFIG_INFOB;

#define NAME_LEN			2

#ifdef __cplusplus
}
#endif


#endif /* COMMON_H_ */
