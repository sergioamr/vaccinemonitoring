/*
 * globals.c
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 *
 *  Global variables refactorization
 */

#include "stdint.h"
#include "i2c.h"
#include "config.h"
#include "lcd.h"
#include "time.h"
#include "stdio.h"
#include "string.h"
#include "timer.h"
#include "globals.h"

char g_szTemp[64];

#pragma SET_DATA_SECTION(".aggregate_vars")
char ATresponse[ATRESP_MAX_LEN] = {};
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".xbigdata_vars")
char	  	  SampleData[SAMPLE_LEN];
#pragma SET_DATA_SECTION()

//volatile uint8_t iStatus = LOG_TIME_STAMP | TEST_FLAG;	//to test the reading and POST formation, there will be no SMS and POST happening
volatile uint16_t iStatus = LOG_TIME_STAMP;			//this is recommended setting
#if MAX_NUM_SENSORS == 5
char SensorName[MAX_NUM_SENSORS][NAME_LEN] = {"A","B","C","D","E"};
uint8_t SensorDisplayName[MAX_NUM_SENSORS] = {0xA,0xB,0xC,0xD,0xE};
#else
char SensorName[MAX_NUM_SENSORS][NAME_LEN] = {"A","B","C","D"};
uint8_t SensorDisplayName[MAX_NUM_SENSORS] = {0xA,0xB,0xC,0xD};
#endif

#pragma SET_DATA_SECTION(".aggregate_vars")
char   acLogData[FILE_BUFFER_LEN];
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoC")
//put all variables that are written less frequently
volatile   int32_t ADCvar[MAX_NUM_SENSORS];
struct 	   tm currTime;
FIL 	   filr;
FIL 	   filw;
FRESULT    fr;
int32_t    iBytesLogged = 0;
int		   g_iCurrDay = 0;
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoD")
//put all variables that are written less frequently
uint32_t 	  g_iAlarmStatus = 0;
double 	 iTemp = 0.0;
int16_t		  g_iAlarmCnfCnt[MAX_NUM_SENSORS+2];		//additional two for power and battery alert

uint8_t iBatteryLevel = 100;
int8_t iSignalLevel  = 99;
//opt
uint8_t iPostSuccess=0;
uint8_t iPostFail=0;
//uint8_t iHTTPRetryFailed=0;
//uint8_t iHTTPRetrySucceeded=0;
uint8_t iHTTPRespDelayCnt=0;
uint8_t iLastDisplayId = 0xFF;
//moved stack variables to prevent stack overflow
uint32_t iUploadTimeElapsed = 0;
uint32_t iSampleTimeElapsed = 0;
uint32_t iSMSRxPollElapsed = 0;
uint32_t iLCDShowElapsed = 0;
uint32_t iMsgRxPollElapsed = 0;
int8_t g_iLastCfgSeq  = -1;
char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN+1];
char signal_gprs=0;
char file_pointer_enabled_gprs_status=0; // for gprs condtition enabling.../// need to be tested..//

#pragma SET_DATA_SECTION()