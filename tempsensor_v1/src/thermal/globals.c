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

const char SensorName[SYSTEM_NUM_SENSORS][NAME_LEN] = { "A", "B", "C", "D", "E" };

/*****************************************************************************************************/
/* Variables to revisit */

volatile uint8_t g_iDebug = 0;

// Current display view
volatile uint8_t g_iDisplayId = 0;

#pragma SET_DATA_SECTION(".helper_vars")
//put all variables that are written less frequently
struct tm g_tmCurrTime;
struct tm g_lastSampleTime;

time_t g_timeNextPull;  // Pulltime from modem every 12 hours
#pragma SET_DATA_SECTION()

