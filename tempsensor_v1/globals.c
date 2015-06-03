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

// Current display view
volatile uint8_t g_iDisplayId = 0;

// Stores several states of the system
volatile uint16_t g_iStatus = LOG_TIME_STAMP;

#pragma SET_DATA_SECTION(".aggregate_vars")
char ATresponse[ATRESP_MAX_LEN] = { };
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".xbigdata_vars")
char SampleData[SAMPLE_LEN];
#pragma SET_DATA_SECTION()

char SensorName[MAX_NUM_SENSORS][NAME_LEN] = { "A", "B", "C", "D", "E" };
uint8_t SensorDisplayName[MAX_NUM_SENSORS] = { 0xA, 0xB, 0xC, 0xD, 0xE };

#pragma SET_DATA_SECTION(".aggregate_vars")
char szLog[FILE_BUFFER_LEN];
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoC")
//put all variables that are written less frequently
volatile int32_t ADCvar[MAX_NUM_SENSORS];
struct tm g_tmCurrTime;
FIL filr;

FRESULT fr;
int32_t iBytesLogged = 0;
int g_iCurrDay = 0;
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoD")
//put all variables that are written less frequently
uint32_t g_iAlarmStatus = 0;
double iTemp = 0.0;
int16_t g_iAlarmCnfCnt[MAX_NUM_SENSORS + 2];//additional two for power and battery alert

uint8_t g_iBatteryLevel = 100;
int8_t iSignalLevel = 99;
//opt
uint8_t iPostSuccess = 0;
uint8_t iPostFail = 0;
int8_t iModemSuccess = -1;
//uint8_t iHTTPRetryFailed=0;
//uint8_t iHTTPRetrySucceeded=0;
uint8_t iHTTPRespDelayCnt = 0;
uint8_t iLastDisplayId = 0xFF;
//moved stack variables to prevent stack overflow
uint32_t iUploadTimeElapsed = 0;
uint32_t iSampleTimeElapsed = 0;
uint32_t iSMSRxPollElapsed = 0;
uint32_t iLCDShowElapsed = 0;
uint32_t iMsgRxPollElapsed = 0;
uint32_t iBootTime = 0;
int8_t g_iLastCfgSeq = -1;
char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN + 1];
char g_iSignal_gprs = 0;
char g_iGprs_network_indication = 0;
char file_pointer_enabled_gprs_status = 0; // for gprs condtition enabling.../// need to be tested..//

#pragma SET_DATA_SECTION()
