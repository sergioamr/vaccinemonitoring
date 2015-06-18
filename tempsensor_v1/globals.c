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

/*****************************************************************************************************/
/* Legacy names */
/*****************************************************************************************************/

const char SensorName[MAX_NUM_SENSORS][NAME_LEN] = { "A", "B", "C", "D", "E" };

/*****************************************************************************************************/
/* Variables to revisit */

volatile uint8_t g_iDebug = 0;

// Current display view
volatile uint8_t g_iDisplayId = 0;

// Stores several states of the system
volatile uint16_t g_iStatus = LOG_TIME_STAMP;

#pragma SET_DATA_SECTION(".helper_vars")
//put all variables that are written less frequently
struct tm g_tmCurrTime;
struct tm g_lastSampleTime;
time_t g_timeNextPull;  // Pulltime from modem every 12 hours
int32_t iBytesLogged = 0;
#pragma SET_DATA_SECTION()

//put all variables that are written less frequently
uint32_t g_iAlarmStatus = 0;
double iTemp = 0.0;

char g_iSignal_gprs = 0;
char g_iGprs_network_indication = 0;
char file_pointer_enabled_gprs_status = 0; // for gprs condtition enabling.../// need to be tested..//

#pragma SET_DATA_SECTION()

char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN + 1];
