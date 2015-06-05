/*
 * thermalcanyon.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_THERMALCANYON_H_
#define TEMPSENSOR_V1_THERMALCANYON_H_

#include <msp430.h>
#include "stdint.h"

#include "config.h"
#include "common.h"
#include "driverlib.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "i2c.h"
#include "timer.h"
#include "modem_uart.h"
#include "battery.h"
#include "rtc.h"
#include "ff.h"
#include "diskio.h"
#include "signal.h"
#include "MMC.h"
#include "pmm.h"
#include "lcd.h"
#include "globals.h"
#include "sms.h"
#include "modem.h"
#include "fatdata.h"
#include "http.h"
#include "stdio.h"
#include "stringutils.h"
#include "temperature.h"

//------------- FUNCTIONS MOVED FROM MAIN - WAITING CLEANUP -------------------

#define FORMAT_FIELD_OFF	1		//2 if field name is 1 character & equal, 3 if field name is 2 character & equal...
extern volatile uint32_t iMinuteTick;
extern char* g_TmpSMScmdBuffer;

void uploadsms();
int8_t sms_process_msg(char* pSMSmsg);
void sampletemp();
void monitoralarm();

extern void thermal_canyon_loop();

extern void encode(double input_val, char output_chars[]); // Encode function from encode.c

#endif /* TEMPSENSOR_V1_THERMALCANYON_H_ */
