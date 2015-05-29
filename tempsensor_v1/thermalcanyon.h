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
#include "math.h"
#include "i2c.h"
#include "timer.h"
#include "uart.h"
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

//------------- FUNCTIONS MOVED FROM MAIN - WAITING CLEANUP -------------------

#define FORMAT_FIELD_OFF	1		//2 if field name is 1 character & equal, 3 if field name is 2 character & equal...
extern volatile uint32_t iMinuteTick;
extern char* g_TmpSMScmdBuffer;
extern char __STACK_END; /* the type does not matter! */
extern char __STACK_SIZE; /* the type does not matter! */

//local functions

static void writetoI2C(uint8_t addressI2C, uint8_t dataI2C);
static float ConvertoTemp(float R);
char* itoa_withpadding(int num);
char* itoa_nopadding(int num);	//TODO remove this function for final release
static void parsetime(char* pDatetime, struct tm* pTime);
static int dopost(char* postdata);

int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize);
void uploadsms();
int8_t processmsg(char* pSMSmsg);
void sendhb();
int dopost_sms_status(void);
int dopost_gprs_connection_status(char status);
void sampletemp();
void modem_init();
void pullTime();
void monitoralarm();

extern void encode(double input_val, char output_chars[]); // Encode function from encode.c

#endif /* TEMPSENSOR_V1_THERMALCANYON_H_ */
