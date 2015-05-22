/*
 * globals.h
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "config.h"
#include "common.h"
#include "ff.h"

#define ATRESP_MAX_LEN		512
#define FILE_BUFFER_LEN     64

extern CONFIG_INFOA* g_pInfoA;
extern CONFIG_INFOB* g_pInfoB;
extern uint32_t 	  g_iAlarmStatus;

extern char SensorName[MAX_NUM_SENSORS][NAME_LEN];
extern char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN+1];
extern uint8_t SensorDisplayName[MAX_NUM_SENSORS];
extern volatile int32_t ADCvar[MAX_NUM_SENSORS];

extern char	ATresponse[ATRESP_MAX_LEN];

extern char	SampleData[SAMPLE_LEN];
extern struct tm currTime;

extern int8_t iSignalLevel;
extern uint8_t iBatteryLevel;
extern char signal_gprs;

extern void ConvertADCToTemperature(unsigned int ADCval, char* TemperatureVal, int8_t iSensorIdx);

extern char tmpstr[10];	//opt
extern char   acLogData[FILE_BUFFER_LEN];

extern char   filler[30]; //ZZZZ recheck to remove this, placeholder for Temperature, which is
                   //now moved to INFOD FRAM to optimize repeated temperature conversion
extern struct tm currTime;
extern FIL filr;
extern FIL filw;
extern FRESULT fr;
extern int32_t iBytesLogged;
extern int g_iCurrYearDay;

extern double iTemp;
extern int16_t g_iAlarmCnfCnt[MAX_NUM_SENSORS+2];		//additional two for power and battery alert

extern uint8_t iBatteryLevel;
extern int8_t iSignalLevel;
//opt
extern uint8_t iPostSuccess;
extern uint8_t iPostFail;
extern uint8_t iHTTPRespDelayCnt;
extern uint8_t iLastDisplayId;

//moved stack variables to prevent stack overflow
extern uint32_t iUploadTimeElapsed;
extern uint32_t iSampleTimeElapsed;
extern uint32_t iSMSRxPollElapsed;
extern uint32_t iLCDShowElapsed;
extern uint32_t iMsgRxPollElapsed;
extern int8_t g_iLastCfgSeq;
extern volatile uint16_t iStatus;

extern char file_pointer_enabled_gprs_status; // for gprs condtition enabling.../// need to be tested..//

#endif /* GLOBALS_H_ */
