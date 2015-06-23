/*
 * globals.h
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_GLOBALS_H_
#define TEMPSENSOR_V1_GLOBALS_H_

#include "config.h"
#include "common.h"
#include "ff.h"
extern volatile uint8_t g_iDebug;

extern CONFIG_DEVICE *g_pDevCfg;
extern CONFIG_CALIBRATION *g_pCalibrationCfg;
extern CONFIG_SYSTEM *g_pSysCfg; // System configuration
extern SYSTEM_STATE *g_pSysState;

extern uint32_t g_iAlarmStatus;
extern volatile uint8_t g_iDisplayId;

extern const char SensorName[SYSTEM_NUM_SENSORS][NAME_LEN];
extern struct tm g_tmCurrTime;

extern int8_t g_iSignalLevel;

extern char g_iSignal_gprs;
extern char g_iSignal_gsm;

extern struct tm g_tmCurrTime;
extern struct tm g_lastSampleTime;
extern int32_t iBytesLogged;

extern double iTemp;

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

extern int8_t iModemSuccess;
extern int8_t g_iLastCfgSeq;
extern volatile uint16_t g_iStatus;

extern char file_pointer_enabled_gprs_status; // for gprs condtition enabling.../// need to be tested..//

#endif /* TEMPSENSOR_V1_GLOBALS_H_ */