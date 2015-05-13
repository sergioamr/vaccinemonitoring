/* --COPYRIGHT--,BSD_EX
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************
 *
 *                       MSP430 CODE EXAMPLE DISCLAIMER
 *
 * MSP430 code examples are self-contained low-level programs that typically
 * demonstrate a single peripheral function or device feature in a highly
 * concise manner. For this the code may rely on the device's power-on default
 * register values and settings such as the clock configuration and care must
 * be taken when combining code from several examples to avoid potential side
 * effects. Also see www.ti.com/grace for a GUI- and www.ti.com/msp430ware
 * for an API functional library-approach to peripheral configuration.
 *
 * --/COPYRIGHT--*/
//******************************************************************************
//  MSP430FR59xx Demo - ADC12, Using an External Reference
//
//  Description: This example shows how to use an external positive reference for
//  the ADC12.The external reference is applied to the VeREF+ pin. AVss is used
//  for the negative reference. A single conversion is performed on channel A0.
//  The conversion results are stored in ADC12MEM0. Test by applying a voltage
//  to channel A0, then setting and running to a break point at the "_NOP()"
//  instruction. To view the conversion results, open an SFR window in debugger
//  and view the contents of ADC12MEM0 or from the variable ADCvar.
//  NOTE: VeREF+ range: 1.4V (min) to AVCC (max)
//        VeREF- range: 0V (min) to 1.2V (max)
//        Differential ref voltage range: 1.4V(min) to AVCC(max)
//          (see datasheet for device specific information)
//
//                MSP430FR5969
//             -------------------
//         /|\|                   |
//          | |                   |
//          --|RST                |
//            |                   |
//     Vin -->|P1.0/A0            |
//            |                   |
//     REF -->|P1.1/VREF+/VeREF+  |
//            |                   |
//
//   T. Witt / P. Thanigai
//   Texas Instruments Inc.
//   November 2011
//   Built with IAR Embedded Workbench V5.30 & Code Composer Studio V5.5
//******************************************************************************
#define SEQUENCE
//#define FILE_TEST
//#define SAMPLE_POST
#define NO_CODE_SIZE_LIMIT
//#define BATTERY_DISABLED
//#define I2C_DISABLED
//#define POWER_SAVING_ENABLED
//#define NOTFROMFILE
#define BUZZER_DISABLED
#define ENABLE_SIM_SLOT		//needed to set on new board, comment it for old board
#define ALERT_UPLOAD_DISABLED
//#define CALIBRATION			//set this flag whenever the device has to undergo calibration
#define MIN 14

//LCD lines
#define LINE1					1
#define LINE2					2
#define GSM 1
#define GPRS 2
//Temperature cut off
#define TEMP_CUTOFF				-800		//-80 deg C
#define MODEM_CHECK_RETRY 	3
#define NAME_LEN			2
#define TEMP_DATA_LEN		5

#define MODEM_TX_DELAY1		1000
#define MODEM_TX_DELAY2		5000

//#define ATRESP_MAX_LEN		128	//opt
#define ATRESP_MAX_LEN		64
#define HTTP_RESPONSE_RETRY	10

#define FILE_BUFFER_LEN     64

#define MAX_NUM_CONTINOUS_SAMPLES 10

#define FILE_NAME			"/data/150623.csv"
#define TS_SIZE				21
#define TS_FIELD_OFFSET		1	//1 - $, 3 - $TS

#define NETWORK_DOWN_SS		14
#define NETWORK_UP_SS		NETWORK_DOWN_SS + 2 //2 points above the network down signal level
#define NETWORK_MAX_SS		31

#define NETWORK_ZERO 10

#include <msp430.h>

#include "config.h"
#include "common.h"
#include "driverlib.h"
#include "stdlib.h"
#include "string.h"
#include "sms.h"
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
#include "encode.h"

#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
#define AGGREGATE_SIZE		256			//288
#define MAX_DISPLAY_ID		9
#else
#define AGGREGATE_SIZE		256
#define MAX_DISPLAY_ID		6
#endif

#define FORMAT_FIELD_OFF	1		//2 if field name is 1 character & equal, 3 if field name is 2 character & equal...
extern volatile uint32_t iMinuteTick;
extern char* g_TmpSMScmdBuffer;
extern char __STACK_END; /* the type does not matter! */
extern char __STACK_SIZE; /* the type does not matter! */

//volatile unsigned int ADCvar[MAX_NUM_SENSORS];
volatile char isConversionDone = 0;
volatile uint8_t iDisplayId = 0;
volatile int iSamplesRead = 0;

//char		  filler[SAMPLE_LEN];
//NOTE placement of data sections is in alphabetical order
#pragma SET_DATA_SECTION(".aggregate_vars")
char ATresponse[ATRESP_MAX_LEN] = { };
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".xbigdata_vars")
char SampleData[SAMPLE_LEN];
#pragma SET_DATA_SECTION()

//volatile uint8_t iStatus = LOG_TIME_STAMP | TEST_FLAG;	//to test the reading and POST formation, there will be no SMS and POST happening
volatile uint16_t iStatus = LOG_TIME_STAMP;		//this is recommended setting
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
char SensorName[MAX_NUM_SENSORS][NAME_LEN] = { "A", "B", "C", "D", "E" };
uint8_t SensorDisplayName[MAX_NUM_SENSORS] = { 0xA, 0xB, 0xC, 0xD, 0xE };
#else
char SensorName[MAX_NUM_SENSORS][NAME_LEN] = {"A","B","C","D"};
uint8_t SensorDisplayName[MAX_NUM_SENSORS] = {0xA,0xB,0xC,0xD};
#endif

#pragma SET_DATA_SECTION(".aggregate_vars")
static char tmpstr[10];	//opt
char acLogData[FILE_BUFFER_LEN];
//char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN+1];
char filler[30]; //ZZZZ recheck to remove this, placeholder for Temperature, which is
//now moved to INFOD FRAM to optimize repeated temperature conversion
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoC")
//put all variables that are written less frequently
volatile int32_t ADCvar[MAX_NUM_SENSORS];
struct tm currTime;
FIL filr;
FIL filw;
FRESULT fr;
int32_t iBytesLogged = 0;
int g_iCurrDay = 0;
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".config_vars_infoD")
//put all variables that are written less frequently
CONFIG_INFOA* g_pInfoA = (CONFIG_INFOA*) INFOA_ADDR;
CONFIG_INFOB* g_pInfoB = (CONFIG_INFOB*) INFOB_ADDR;
uint32_t g_iAlarmStatus = 0;
//CONFIG_INFOA* pstCfgInfoA = INFOA_ADDR;
double iTemp = 0.0;
int16_t g_iAlarmCnfCnt[MAX_NUM_SENSORS + 2]; //additional two for power and battery alert

uint8_t iBatteryLevel = 100;
int8_t iSignalLevel = 99;
//opt
uint8_t iPostSuccess = 0;
uint8_t iPostFail = 0;
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
int8_t g_iLastCfgSeq = -1;
char Temperature[MAX_NUM_SENSORS][TEMP_DATA_LEN + 1];
char signal_gprs = 0;
char file_pointer_enabled_gprs_status = 0; // for gprs condtition enabling.../// need to be tested..//

#pragma SET_DATA_SECTION()

//local functions
static void writetoI2C(uint8_t addressI2C, uint8_t dataI2C);
static float ConvertoTemp(float R);
static void ConvertADCToTemperature(unsigned int ADCval, char* TemperatureVal,
		int8_t iSensorIdx);
char* itoa(int num);
char* itoa_nopadding(int num);	//ZZZZ remove this function for final release
static void parsetime(char* pDatetime, struct tm* pTime);
static int dopost(char* postdata);
static FRESULT logsampletofile(FIL* fobj, int* tbw);
int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize);
void uploadsms();
int8_t processmsg(char* pSMSmsg);
void sendhb();
int dopost_sms_status(void);
int dopost_gprs_connection_status(char status);
void lcd_init();
void lcd_show(int8_t iItemId);
void lcd_print(char* pcData);
void lcd_print_line(char* pcData, int8_t iLine);
void lcd_reset();
void lcd_blenable();
void lcd_bldisable();
void lcd_clear();
void lcd_on();
void lcd_off();
void lcd_setaddr(int8_t addr);
#ifdef POWER_SAVING_ENABLED
static int8_t modem_enter_powersave_mode();
static int8_t modem_exit_powersave_mode();
#endif

FATFS FatFs; /* Work area (file system object) for logical drive */

DWORD get_fattime(void) {
	DWORD tmr;

	rtc_getlocal(&currTime);
	/* Pack date and time into a DWORD variable */
	tmr = (((DWORD) currTime.tm_year - 1980) << 25)
			| ((DWORD) currTime.tm_mon << 21) | ((DWORD) currTime.tm_mday << 16)
			| (WORD) (currTime.tm_hour << 11) | (WORD) (currTime.tm_min << 5)
			| (WORD) (currTime.tm_sec >> 1);
	return tmr;
}

void deactivatehttp() {
	uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
	delay(MODEM_TX_DELAY2);
}

void dohttpsetup() { // needs char* apn
	uart_tx("AT+CGDCONT=1,\"IP\",\"airtelgprs.com\",\"0.0.0.0\",0,0\r\n"); //APN
	//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n");
	delay(MODEM_TX_DELAY2);

	uart_tx("AT#SGACT=1,1\r\n");
	delay(MODEM_TX_DELAY2);

	uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
	delay(MODEM_TX_DELAY2);
}

void modem_init(int8_t slot) {
#ifdef ENABLE_SIM_SLOT
	if (slot != 2) {
		//enable SIM A (slot 1)
		uart_tx("AT#GPIO=2,0,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#GPIO=4,1,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#GPIO=3,0,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#SIMDET=0\r\n");
		delay(5000);
		uart_tx("AT#SIMDET=1\r\n");
		delay(MODEM_TX_DELAY1);
	} else {
		//enable SIM B (slot 2)
		uart_tx("AT#GPIO=2,1,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#GPIO=4,0,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#GPIO=3,1,1\r\n");
		delay(MODEM_TX_DELAY1);
		uart_tx("AT#SIMDET=0\r\n");
		delay(5000);
		uart_tx("AT#SIMDET=1\r\n");
		delay(MODEM_TX_DELAY1);
	}
#endif
	uart_tx("AT+CMGF=1\r\n");		   // set sms format to text mode
	delay(MODEM_TX_DELAY1);
	uart_tx("AT+CMEE=2\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT#CMEEMODE=1\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT&K4\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT&P0\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT&W0\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT#NITZ=1\r\n");
	delay(MODEM_TX_DELAY1);
	uart_tx("AT+CSDH=1\r\n");
	delay(MODEM_TX_DELAY1);
	delay(10000);		//some time to enable SMS,POST to work
}

//WARNING: doget() should not be used in parallel to HTTP POST
int doget(char* queryData) {
	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_EXTENDED_LEN;
	RX[RX_EXTENDED_LEN + 1] = 0;	//null termination
#if 0
			strcpy(queryData,"AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/358072043113601/1/\"\r\n");	//reuse,   //SERIAL
#else
	strcpy(queryData, "AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/");
	strcat(queryData, g_pInfoA->cfgIMEI);
	strcat(queryData, "/1/\"\r\n");
#endif

	uart_tx(queryData);
	delay(10000);	//opt
	uart_tx("AT#HTTPRCV=1\r\n");		//get the configuartion
	delay(10000); //opt
	memset(queryData, 0, CFG_SIZE);
	uart_rx(ATCMD_HTTPRCV, queryData);

	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;
	return 0;
}

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

void monitoralarm() {
	CONFIG_INFOA* pstCfgInfoA = (CONFIG_INFOA*) INFOA_ADDR;

	//CONFIG_INFOA* pstCfgInfoA = SampleData;
	int8_t iCnt = 0;

	//iterate through sensors
	for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
		//get sensor value
		//temp = &Temperature[iCnt];

		//check sensor was plugged out
		if (Temperature[iCnt][1] == '-') {
			TEMP_ALARM_CLR(iCnt);
			g_iAlarmCnfCnt[iCnt] = 0;
			continue;
		}

		iTemp = strtod(&Temperature[iCnt], NULL);
		//iTemp = strtod("24.5",NULL);
		//check for low temp threshold
		if (iTemp < pstCfgInfoA->stTempAlertParams[iCnt].threshcold) {
			//check if alarm is set for low temp
			if (TEMP_ALARM_GET(iCnt) != LOW_TEMP_ALARM_ON) {
				//set it off incase it was earlier confirmed
				TEMP_ALARM_CLR(iCnt);
				//set alarm to low temp
				TEMP_ALARM_SET(iCnt, LOW_TEMP_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == LOW_TEMP_ALARM_ON) {
				//low temp alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= pstCfgInfoA->stTempAlertParams[iCnt].mincold) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
#ifndef ALERT_UPLOAD_DISABLED
					if(!(iStatus & BACKLOG_UPLOAD_ON))
					{
						iStatus |= ALERT_UPLOAD_ON;	//indicate to upload sampled data if backlog is not progress
					}
#endif
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms if not send already
					if(!(iStatus & SMSED_LOW_TEMP))
					{
						memset(SampleData,0,SMS_ENCODED_LEN);
						strcat(SampleData, "Alert Sensor ");
						strcat(SampleData, SensorName[iCnt]);
						strcat(SampleData,": Temp too LOW for ");
						strcat(SampleData,itoa(pstCfgInfoA->stTempAlertParams[iCnt].mincold));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately.");	//superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
						iLen = strlen(SampleData);
						SampleData[iLen] = (char)0x1A;

						sendmsg(SampleData);

						iStatus |= SMSED_LOW_TEMP;
					}
#endif

					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}
		} else if (iTemp > pstCfgInfoA->stTempAlertParams[iCnt].threshhot) {
			//check if alarm is set for high temp
			if (TEMP_ALARM_GET(iCnt) != HIGH_TEMP_ALARM_ON) {
				//set it off incase it was earlier confirmed
				TEMP_ALARM_CLR(iCnt);
				//set alarm to high temp
				TEMP_ALARM_SET(iCnt, HIGH_TEMP_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == HIGH_TEMP_ALARM_ON) {
				//high temp alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= pstCfgInfoA->stTempAlertParams[iCnt].minhot) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
#ifndef ALERT_UPLOAD_DISABLED
					if(!(iStatus & BACKLOG_UPLOAD_ON))
					{
						iStatus |= ALERT_UPLOAD_ON;	//indicate to upload sampled data if backlog is not progress
					}
#endif
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms if not send already
					if(!(iStatus & SMSED_HIGH_TEMP))
					{
						memset(SampleData,0,SMS_ENCODED_LEN);
						strcat(SampleData, "Alert Sensor ");
						strcat(SampleData, SensorName[iCnt]);
						strcat(SampleData,": Temp too HIGH for ");
						strcat(SampleData,itoa(pstCfgInfoA->stTempAlertParams[iCnt].minhot));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately."); //superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
						iLen = strlen(SampleData);
						SampleData[iLen] = (char)0x1A;

						sendmsg(SampleData);

						iStatus |= SMSED_HIGH_TEMP;
					}
#endif
					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}

		} else {
			//check if alarm is set
			if (TEMP_ALARM_GET(iCnt) != TEMP_ALERT_OFF) {
				//reset the alarm
				TEMP_ALARM_CLR(iCnt);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			}
		}
	}

	//check for battery alert
	iCnt = MAX_NUM_SENSORS;
	if (iBatteryLevel < pstCfgInfoA->stBattPowerAlertParam.battthreshold) {
		//check if battery alarm is set
		if (TEMP_ALARM_GET(iCnt) != BATT_ALARM_ON) {
			TEMP_ALARM_CLR(iCnt);//reset the alarm in case it was earlier confirmed
			//set battery alarm
			TEMP_ALARM_SET(iCnt, BATT_ALARM_ON);
			//initialize confirmation counter
			g_iAlarmCnfCnt[iCnt] = 0;
		} else if (TEMP_ALARM_GET(iCnt) == BATT_ALARM_ON) {
			//power alarm is already set, increment the counter
			g_iAlarmCnfCnt[iCnt]++;
			if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
					>= pstCfgInfoA->stBattPowerAlertParam.minutesbathresh) {
				TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
				//set buzzer if not set
				if (!(iStatus & BUZZER_ON)) {
					//P3OUT |= BIT4;
					iStatus |= BUZZER_ON;
				}
#ifdef SMS_ALERT
				//send sms LOW Battery: ColdTrace has now 15% battery left. Charge your device immediately.
				memset(SampleData,0,SMS_ENCODED_LEN);
				strcat(SampleData, "LOW Battery: ColdTrace has now ");
				strcat(SampleData,itoa(iBatteryLevel));
				strcat(SampleData, "battery left. Charge your device immediately.");
				iLen = strlen(SampleData);
				SampleData[iLen] = (char)0x1A;

				sendmsg(SampleData);
#endif

				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;

			}
		}
	} else {
		//reset battery alarm
		TEMP_ALARM_SET(iCnt, BATT_ALERT_OFF);
		g_iAlarmCnfCnt[iCnt] = 0;
	}

	//check for power alert enable
	if (pstCfgInfoA->stBattPowerAlertParam.enablepoweralert) {
		iCnt = MAX_NUM_SENSORS + 1;
		if (P4IN & BIT4) {
			//check if power alarm is set
			if (TEMP_ALARM_GET(iCnt) != POWER_ALARM_ON) {
				TEMP_ALARM_CLR(iCnt); //reset the alarm in case it was earlier confirmed
				//set power alarm
				TEMP_ALARM_SET(iCnt, POWER_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == POWER_ALARM_ON) {
				//power alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= pstCfgInfoA->stBattPowerAlertParam.minutespower) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.
					memset(SampleData,0,SMS_ENCODED_LEN);
					strcat(SampleData, "Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.");
					iLen = strlen(SampleData);
					SampleData[iLen] = (char)0x1A;

					sendmsg(SampleData);
#endif
					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}
		} else {
			//reset power alarm
			TEMP_ALARM_SET(iCnt, POWER_ALERT_OFF);
			g_iAlarmCnfCnt[iCnt] = 0;
		}
	}

	//disable buzzer if it was ON and all alerts are gone
	if ((iStatus & BUZZER_ON) && (g_iAlarmStatus == 0)) {
		iStatus &= ~BUZZER_ON;
	}

}

void sampletemp() {
	int iIdx = 0;
	int iLastSamplesRead = 0;
	iSamplesRead = 0;

	//initialze ADCvar
	for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
		ADCvar[iIdx] = 0;
	}

	for (iIdx = 0; iIdx < SAMPLE_COUNT; iIdx++) {
		ADC12CTL0 &= ~ADC12ENC;
		ADC12CTL0 |= ADC12ENC | ADC12SC;
		while ((iSamplesRead - iLastSamplesRead) == 0)
			;
		iLastSamplesRead = iSamplesRead;
//		delay(10);	//to allow the ADC conversion to complete
	}

	if ((isConversionDone) && (iSamplesRead > 0)) {
		for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
			ADCvar[iIdx] /= iSamplesRead;
		}
	}

}

/*_Sigfun * signal(int i, _Sigfun *proc)
 {
 __no_operation();

 }*/

int main(void) {
	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	int iIdx = 0;
	int iPOSTstatus = 0;
	int32_t dwLastseek = 0;
	int32_t dwFinalSeek = 0;
	int32_t iSize = 0;
	int16_t iOffset = 0;
	int8_t dummy = 0;
	int8_t iSampleCnt = 0;
	char gprs_network_indication = 0;
	int32_t dw_file_pointer_back_log = 0; // for error condition /// need to be tested.
	char file_pointer_enabled_sms_status = 0; // for sms condtition enabling.../// need to be tested
	CONFIG_INFOA* pstCfgInfoA = NULL;

	WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT

	//ZZZZ for stack usage calculation. Should be removed during the release
	//*(int32_t*)(&__STACK_END - &__STACK_SIZE) = 0xdeadbeef;
	iIdx = &__STACK_SIZE;
	memset((void*) (&__STACK_END - &__STACK_SIZE), 0xa5, iIdx / 4); //paint 1/4th stack with know values

	dummy = sizeof(CONFIG_INFOA);
	dummy = sizeof(CONFIG_INFOB);
	dummy = sizeof(Temperature);

	// Configure GPIO
	P2DIR |= BIT3;							// SPI CS
	P2OUT |= BIT3;					// drive SPI CS high to deactive the chip
	P2SEL1 |= BIT4 | BIT5 | BIT6;             // enable SPI CLK, SIMO, SOMI
	PJSEL0 |= BIT4 | BIT5;                    // For XT1
	P1SELC |= BIT0 | BIT1;                    // Enable VEREF-,+
	P1SELC |= BIT2;                           // Enable A/D channel A2
#ifdef SEQUENCE
	P1SELC |= BIT3 | BIT4 | BIT5;          // Enable A/D channel A3-A5
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	P4SELC |= BIT2;          				// Enable A/D channel A10
#endif
#endif
	P1SEL1 |= BIT6 | BIT7;					// Enable I2C SDA and CLK

	P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
	P2SEL0 &= ~(BIT0 | BIT1);
	P4DIR |= BIT0 | BIT5 | BIT7; // Set P4.0 (Modem reset), LEDs to output direction
	P4OUT &= ~BIT0;                           // Reset high

	P3DIR |= BIT4;      						// Set P3.4 buzzer output
	P4IE |= BIT1;							// enable interrupt for button input
	P2IE |= BIT2;							// enable interrupt for buzzer off

	PJDIR |= BIT6 | BIT7;      			// set LCD reset and Backlight enable
	PJOUT |= BIT6;							// LCD reset pulled high
	PJOUT &= ~BIT7;							// Backlight disable

#ifdef POWER_SAVING_ENABLED
	P3DIR |= BIT3;							// Modem DTR
	P3OUT &= ~BIT3;// DTR ON (low)
#endif
	//P1SEL0 |= BIT2;

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	// Startup clock system with max DCO setting ~8MHz
	CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
	CSCTL1 = DCOFSEL_3 | DCORSEL;             // Set DCO to 8MHz
	CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
	//CSCTL3 = DIVA__1 | DIVS__8 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL4 &= ~LFXTOFF;
	do {
		CSCTL5 &= ~LFXTOFFG;                    // Clear XT1 fault flag
		SFRIFG1 &= ~OFIFG;
	} while (SFRIFG1 & OFIFG);                   // Test oscillator fault flag
	CSCTL0_H = 0;                             // Lock CS registers

	// Configure ADC12
	ADC12CTL0 = ADC12ON | ADC12SHT0_2;       // Turn on ADC12, set sampling time
	ADC12CTL1 = ADC12SHP;                     // Use pulse mode
	ADC12CTL2 = ADC12RES_2;                   // 12bit resolution
	ADC12CTL3 |= ADC12CSTARTADD_2;			// A2 start channel
	ADC12MCTL2 = ADC12VRSEL_4 | ADC12INCH_2; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 2
#ifdef SEQUENCE
	ADC12MCTL3 = ADC12VRSEL_4 | ADC12INCH_3; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 3
	ADC12MCTL4 = ADC12VRSEL_4 | ADC12INCH_4; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 4
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
	ADC12MCTL6 = ADC12VRSEL_4 | ADC12INCH_10 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#else
			ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#endif
	ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;                   // sample a sequence
	ADC12CTL0 |= ADC12MSC; //first sample by trigger and rest automatic trigger by prior conversion
#endif
	//ADC interrupt logic
	//ZZZZ comment ADC for debugging other interfaces
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5 | ADC12IE6; // Enable ADC conv complete interrupt
#else
			ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5; // Enable ADC conv complete interrupt
#endif

	// Configure USCI_A0 for UART mode
	UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
	UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	// Baud Rate calculation
	// 8000000/(16*115200) = 4.340	//4.340277777777778
	// Fractional portion = 0.340
	// User's Guide Table 21-4: UCBRSx = 0x49
	// UCBRFx = int ( (4.340-4)*16) = 5
	UCA0BRW = 4;                             // 8000000/16/115200
	UCA0MCTLW |= UCOS16 | UCBRF_5 | 0x4900;

#ifdef LOOPBACK
	UCA0STATW |= UCLISTEN;
#endif
	UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI

	UCA0IE |= UCRXIE;                // Enable USCI_A0 RX interrupt

#ifndef I2C_DISABLED
	//i2c_init(EUSCI_B_I2C_SET_DATA_RATE_400KBPS);
	i2c_init(380000);

#endif

	__bis_SR_register(GIE);		//enable interrupt globally

	delay(1000);
	lcd_reset();
	lcd_blenable();

#if UTC_TEST
	currTime.tm_year = 2015;
	currTime.tm_mon = 8;
	currTime.tm_mday = 1;
	currTime.tm_hour = 0;
	currTime.tm_min = 30;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);

	currTime.tm_year = 2015;
	currTime.tm_mon = 9;
	currTime.tm_mday = 2;
	currTime.tm_hour = 4;
	currTime.tm_min = 12;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);

	currTime.tm_year = 2015;
	currTime.tm_mon = 7;
	currTime.tm_mday = 30;
	currTime.tm_hour = 3;
	currTime.tm_min = 57;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);

	currTime.tm_year = 2016;
	currTime.tm_mon = 3;
	currTime.tm_mday = 19;
	currTime.tm_hour = 2;
	currTime.tm_min = 19;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);

	currTime.tm_year = 2014;
	currTime.tm_mon = 4;
	currTime.tm_mday = 22;
	currTime.tm_hour = 5;
	currTime.tm_min = 30;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);

	currTime.tm_year = 2014;
	currTime.tm_mon = 4;
	currTime.tm_mday = 30;
	currTime.tm_hour = 5;
	currTime.tm_min = 31;
	currTime.tm_sec = 0;
	converttoUTC(&currTime);
#endif
	//filler[0]=0xff;

#ifdef FILE_TEST
	{
		FIL fil; /* File object */
		FRESULT fr; /* FatFs return code */
		int br,bw;

		/* Register work area to the default drive */
		f_mount(&FatFs, "", 0);

		/* Open a text file */
		fr = f_open(&fil, "150218.txt", FA_READ|FA_WRITE|FA_OPEN_ALWAYS);
		if (fr) return (int)fr;

		/* Read all lines and display it */
		//while (f_gets(line, sizeof line, &fil))
		//    printf(line);
		/* Close the file */

		iIdx = 0;
		while(iIdx < 5)
		{
#ifdef WRITE
			memset(SampleData,iIdx+ 0x30,sizeof(SampleData));
			fr = f_write(&fil, SampleData, sizeof(SampleData), &bw); /* Read a chunk of source file */
			iIdx++;
#else
			//f_lseek(&fil,512);
			fr = f_read(&fil, acLogData, sizeof(acLogData), &br); /* Read a chunk of source file */
			iIdx++;
#endif
		}

		f_close(&fil);
	}
#endif

#ifndef BATTERY_DISABLED
	batt_init();
#endif
	delay(1000);
	lcd_init();
	delay(1000);
	lcd_print("Starting...");

	//while(1)
	//{
	//iBatteryLevel = batt_getlevel();
	//delay(2000);
	//}

	//i2c_init(SLAVE_ADDR_DISPLAY,EUSCI_B_I2C_SET_DATA_RATE_400KBPS);

	//Configure 7 segement LEDs
	writetoI2C(0x01, 0x0f);						// Decode Mode register

	writetoI2C(0x02, 0x0B);						//Intensity register

	writetoI2C(0x04, 0x01);						//Configuration Register

	writetoI2C(0x20, 0x0A);						//Display channel

#ifdef NO_CODE_SIZE_LIMIT
	writetoI2C(0x21, 4);

	writetoI2C(0x22, 2);

	writetoI2C(0x23, 0);

	writetoI2C(0x24, 0x04);
#endif

	//ADC12CTL0 |= ADC12ENC;                    // Enable conversions
	//ADC12CTL0 |= ADC12ENC | ADC12SC;            // Start conversion-software trigger
	sampletemp();
	delay(2000); // to allow conversion to get over and prevent any side-effects to other interface like modem

	//check Modem is powered on
	for (iIdx = 0; iIdx < MODEM_CHECK_RETRY; iIdx++) {
		if ((P4IN & BIT0) == 0) {
			iStatus |= MODEM_POWERED_ON;
			break;
		} else {
			iStatus &= ~MODEM_POWERED_ON;
			delay(100);
		}
	}

	if (iStatus & MODEM_POWERED_ON) {

		modem_init(g_pInfoA->cfgSIMSlot);

#ifdef POWER_SAVING_ENABLED
		uart_tx("AT+CFUN=5\r\n"); //full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);

		uart_tx("AT+CFUN?\r\n");//full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);
#endif

		RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
		uart_tx("AT+CCLK?\r\n");
		delay(1000);
#if 0
		memset(ATresponse,0,sizeof(ATresponse));
		strcat(ATresponse,"$ST,a=1,b=xyz,$EN");
		iIdx = strlen(ATresponse);
		ATresponse[iIdx] = 0x1A;
		sendmsg(ATresponse);
		delay(1000);
#endif

		memset(ATresponse, 0, sizeof(ATresponse));
		uart_rx(ATCMD_CCLK, ATresponse);
		parsetime(ATresponse, &currTime);
		rtc_init(&currTime);
		g_iCurrDay = currTime.tm_mday;
		//ZZZZ determine the file name for SD logging

		uart_tx("AT+CGSN\r\n");
		delay(MODEM_TX_DELAY1);

		uart_tx("AT+CNUM\r\n");
		delay(MODEM_TX_DELAY1);

		RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
		uart_tx("AT+CSQ\r\n");
		delay(MODEM_TX_DELAY1);
		memset(ATresponse, 0, sizeof(ATresponse));
		uart_rx(ATCMD_CSQ, ATresponse);
		if (ATresponse[0] != 0) {
			iSignalLevel = strtol(ATresponse, 0, 10);
			//  if((iSignalLevel <= NETWORK_DOWN_SS) || (iSignalLevel >= NETWORK_MAX_SS)){
			//  	signal_gprs=0;
			//  }
			//   else{
			//      signal_gprs=1;
			//  }
		}
		/// added for gprs connection..//
		signal_gprs = dopost_gprs_connection_status(GPRS);

		// added for IMEI number//
		if ((uint8_t) g_pInfoA->cfgIMEI[0] == 0xFF) {
			RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
			uart_tx("AT+CGSN\r\n");
			delay(MODEM_TX_DELAY1);
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CGSN, ATresponse);
			if (memcmp(ATresponse, "INVALID", strlen("INVALID"))) {
				pstCfgInfoA = (CONFIG_INFOA*)SampleData;
				memcpy(pstCfgInfoA, (void*) INFOA_ADDR, sizeof(CONFIG_INFOA));
				//copy valid IMEI to FRAM
				memcpy(pstCfgInfoA->cfgIMEI, ATresponse,
						strlen(ATresponse) + 1);
				FRAMCtl_write8(pstCfgInfoA, (void*) INFOA_ADDR,
						sizeof(CONFIG_INFOA));
			}
		}
		uart_tx("ATE0\r\n");
		delay(MODEM_TX_DELAY1);

		//delete all existing SMS messages
		//delallmsg();
		//delay(5000);
		//heartbeat
		sendhb();

	}

#ifndef BATTERY_DISABLED
	iBatteryLevel = batt_getlevel();
#else
	iBatteryLevel = 0;
#endif

	/* Register work area to the default drive */
	f_mount(&FatFs, "", 0);

	//get the last read offset from FRAM
	if (g_pInfoB->dwLastSeek > 0) {
		dwLastseek = g_pInfoB->dwLastSeek;
	}

	iUploadTimeElapsed = iMinuteTick;		//initialize POST minute counter
	iSampleTimeElapsed = iMinuteTick;
	iSMSRxPollElapsed = iMinuteTick;
	iLCDShowElapsed = iMinuteTick;
	iMsgRxPollElapsed = iMinuteTick;
	iDisplayId = 0;
	while (1) {
		//check if conversion is complete
		if ((isConversionDone) || (iStatus & TEST_FLAG)) {
			//convert the current sensor ADC value to temperature
			for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
				memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
				ConvertADCToTemperature(ADCvar[iIdx], &Temperature[iIdx], iIdx);
			}

			//Write temperature data to 7 segment display
			writetoI2C(0x20, SensorDisplayName[iDisplayId]);//Display channel
			writetoI2C(0x21, Temperature[iDisplayId][0]);//(A0tempdegCint >> 8));
			writetoI2C(0x22, Temperature[iDisplayId][1]);//(A0tempdegCint >> 4));
			writetoI2C(0x23, Temperature[iDisplayId][3]);	//(A0tempdegCint));
			writetoI2C(0x24, 0x04);

#ifndef  NOTFROMFILE
			//log to file
			fr = logsampletofile(&filw, &iBytesLogged);
			if (fr == FR_OK) {
				iSampleCnt++;
				if (iSampleCnt >= MAX_NUM_CONTINOUS_SAMPLES) {
					iStatus |= LOG_TIME_STAMP;
					iSampleCnt = 0;
				}
			}

#endif
			//monitor for temperature alarms
			monitoralarm();

#ifdef SMS
			memset(MSGData,0,sizeof(MSGData));
			strcat(MSGData,SensorName[iDisplayId]);
			strcat(MSGData," ");
			strcat(MSGData,Temperature);
			iIdx = strlen(MSGData);
			MSGData[iIdx] = 0x1A;
#endif
			//sendmsg(MSGData);
			isConversionDone = 0;

			//if(iIdx >= SAMPLE_PERIOD)
			//if(((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) || (iStatus & TEST_FLAG) || (iStatus & ALERT_UPLOAD_ON))
			if ((((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod)
					|| (iStatus & TEST_FLAG) || (iStatus & BACKLOG_UPLOAD_ON)
					|| (iStatus & ALERT_UPLOAD_ON))
					&& !(iStatus & NETWORK_DOWN)) {
				lcd_print_line("Transmitting....", LINE2);
				//iStatus &= ~TEST_FLAG;
#ifdef SMS_ALERT
				iStatus &= ~SMSED_HIGH_TEMP;
				iStatus &= ~SMSED_LOW_TEMP;
#endif
				if ((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) {
					//if(((g_iUploadPeriod/g_iSamplePeriod) < MAX_NUM_CONTINOUS_SAMPLES) ||
					//    (iStatus & ALERT_UPLOAD_ON))
					if ((g_iUploadPeriod / g_iSamplePeriod)
							< MAX_NUM_CONTINOUS_SAMPLES) {
						//trigger a new timestamp
						iStatus |= LOG_TIME_STAMP;
						iSampleCnt = 0;
						//iStatus &= ~ALERT_UPLOAD_ON;	//reset alert upload indication
					}

					iUploadTimeElapsed = iMinuteTick;
				} else if ((iStatus & ALERT_UPLOAD_ON)
						&& (iSampleCnt < MAX_NUM_CONTINOUS_SAMPLES)) {
					//trigger a new timestamp
					iStatus |= LOG_TIME_STAMP;
					iSampleCnt = 0;
				}

				//reset the alert uplaod indication
				if (iStatus & ALERT_UPLOAD_ON) {
					iStatus &= ~ALERT_UPLOAD_ON;
				}

#ifdef POWER_SAVING_ENABLED
				modem_exit_powersave_mode();
#endif

				if (!(iStatus & TEST_FLAG)) {

					uart_tx(
							"AT+CGDCONT=1,\"IP\",\"airtelgprs.com\",\"0.0.0.0\",0,0\r\n"); //APN
					//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#SGACT=1,1\r\n");
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
					delay(MODEM_TX_DELAY2);
				}

#ifdef NOTFROMFILE
				iPOSTstatus = 0;	//set to 1 if post and sms should happen
				memset(SampleData,0,sizeof(SampleData));
				strcat(SampleData,"IMEI=358072043113601&phone=8455523642&uploadversion=1.20140817.1&sensorid=0|1|2|3&");//SERIAL
				rtc_get(&currTime);
				strcat(SampleData,"sampledatetime=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,itoa(currTime.tm_year)); strcat(SampleData,"/");
					strcat(SampleData,itoa(currTime.tm_mon)); strcat(SampleData,"/");
					strcat(SampleData,itoa(currTime.tm_mday)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_hour)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_min)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_sec));
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				strcat(SampleData,"interval=10|10|10|10&");

				strcat(SampleData,"temp=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,&Temperature[iIdx]);
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				pcData = itoa(batt_getlevel());;
				strcat(SampleData,"batterylevel=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,pcData);
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				if(P4IN & BIT4)
				{
					strcat(SampleData,"powerplugged=0|0|0|0");
				}
				else
				{
					strcat(SampleData,"powerplugged=1|1|1|1");
				}
#else
				//disable UART RX as RX will be used for HTTP POST formating
				pcTmp = pcData = pcSrc1 = pcSrc2 = NULL;
				iIdx = 0;
				iPOSTstatus = 0;
				fr = FR_DENIED;
				iOffset = 0;

				//fr = f_read(&filr, acLogData, 1, &iIdx);  /* Read a chunk of source file */
				memset(ATresponse, 0, AGGREGATE_SIZE + 1); //ensure the buffer in aggregate_var section is more than AGGREGATE_SIZE
				fr = f_open(&filr, FILE_NAME, FA_READ | FA_OPEN_ALWAYS);
				if (fr == FR_OK) {
					dw_file_pointer_back_log = dwLastseek; // added for dummy storing///
					//seek if offset is valid and not greater than existing size else read from the beginning
					if ((dwLastseek != 0) && (filr.fsize >= dwLastseek)) {
						f_lseek(&filr, dwLastseek);
					} else {
						dwLastseek = 0;
					}

					iOffset = dwLastseek % SECTOR_SIZE;
					//check the position is in first half of sector
					if ((SECTOR_SIZE - iOffset) > AGGREGATE_SIZE) {
						fr = f_read(&filr, ATresponse, AGGREGATE_SIZE, &iIdx); /* Read first chunk of sector*/
						if ((fr == FR_OK) && (iIdx > 0)) {
							iStatus &= ~SPLIT_TIME_STAMP; //clear the last status of splitted data
							pcData = (char*)FatFs.win;	//reuse the buffer maintained by the file system
							//check for first time stamp
							//pcTmp = strstr(&pcData[iOffset],"$TS");
							pcTmp = strstr(&pcData[iOffset], "$"); //to prevent $TS rollover case
							if ((pcTmp) && (pcTmp < &pcData[SECTOR_SIZE])) {
								iIdx = (int32_t)pcTmp; //start position
								//check for second time stamp
								//pcTmp = strstr(&pcTmp[TS_FIELD_OFFSET],"$TS");
								pcTmp = strstr(&pcTmp[TS_FIELD_OFFSET], "$");
								if ((pcTmp) && (pcTmp < &pcData[SECTOR_SIZE])) {
									iPOSTstatus = (int32_t)pcTmp; //first src end postion
									iStatus &= ~SPLIT_TIME_STAMP; //all data in FATFS buffer
								} else {
									iPOSTstatus = (int32_t)&pcData[SECTOR_SIZE];	//mark first source end position as end of sector boundary
									iStatus |= SPLIT_TIME_STAMP;
								}
								pcTmp = (char*)iIdx;//re-initialize to first time stamp

								//is all data within FATFS
								if (!(iStatus & SPLIT_TIME_STAMP)) {
									//first src is in FATFS buffer, second src is NULL
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = dwLastseek
											+ (iPOSTstatus - (int32_t) pcSrc1);	//seek to the next time stamp
								}
								//check to read some more data
								else if ((filr.fsize - dwLastseek)
										> (SECTOR_SIZE - iOffset)) {
									//update the seek to next sector
									dwLastseek = dwLastseek + SECTOR_SIZE
											- iOffset;
									//seek
									//f_lseek(&filr, dwLastseek);
									//fr = f_read(&filr, ATresponse, AGGREGATE_SIZE, &iIdx);  /* Read next data of AGGREGATE_SIZE */
									//if((fr == FR_OK) && (iIdx > 0))
									//if(disk_read_ex(0,ATresponse,filr.dsect+1,AGGREGATE_SIZE) == RES_OK)
									if (disk_read_ex(0, ATresponse,
											filr.dsect + 1, 512) == RES_OK) {
										//calculate bytes read
										iSize = filr.fsize - dwLastseek;
										if (iSize >= AGGREGATE_SIZE) {
											iIdx = AGGREGATE_SIZE;
										} else {
											iIdx = iSize;
										}
										//update final lseek for next sample
										//pcSrc1 = strstr(ATresponse,"$TS");
										pcSrc1 = strstr(ATresponse, "$");
										if (pcSrc1) {
											dwFinalSeek = dwLastseek
													+ (pcSrc1 - ATresponse); //seek to the next TS
											iIdx = (int32_t)pcSrc1; //second src end position
										}
										//no next time stamp found
										else {
											dwFinalSeek = dwLastseek + iIdx; //update with bytes read
											dwLastseek = (int32_t)&ATresponse[iIdx]; //get postion of last byte
											iIdx = dwLastseek; //end position for second src
										}
										//first src is in FATFS buffer, second src is ATresponse
										pcSrc1 = pcTmp;
										pcSrc2 = ATresponse;
									}
								} else {
									//first src is in FATFS buffer, second src is NULL
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = filr.fsize; //EOF - update with file size

								}
							} else {
								//control should not come here ideally
								//update the seek to next sector to skip the bad logged data
								dwLastseek = dwLastseek + SECTOR_SIZE - iOffset;

							}
						} else {
							//file system issue ZZZZ
						}
					} else {
						//the position is second half of sector
						fr = f_read(&filr, ATresponse, SECTOR_SIZE - iOffset,
								&iIdx); /* Read data till the end of sector */
						if ((fr == FR_OK) && (iIdx > 0)) {
							iStatus &= ~SPLIT_TIME_STAMP; //clear the last status of splitted data
							//get position of first time stamp
							//pcTmp = strstr(ATresponse,"$TS");
							pcTmp = strstr(ATresponse, "$");

							if ((pcTmp) && (pcTmp < &ATresponse[iIdx])) {
								//check there are enough bytes to check for second time stamp postion
								if (iIdx > TS_FIELD_OFFSET) {
									//check if all data is in ATresponse
									//pcSrc1 = strstr(&ATresponse[TS_FIELD_OFFSET],"$TS");
									pcSrc1 = strstr(&pcTmp[TS_FIELD_OFFSET],
											"$");
									if ((pcSrc1)
											&& (pcSrc1 < &ATresponse[iIdx])) {
										iPOSTstatus = (int32_t)pcSrc1; //first src end position;
										iStatus &= ~SPLIT_TIME_STAMP; //all data in FATFS buffer
									} else {
										iPOSTstatus = (int32_t)&ATresponse[iIdx]; //first src end position;
										iStatus |= SPLIT_TIME_STAMP;
									}
								} else {
									iPOSTstatus = (int32_t)&ATresponse[iIdx]; //first src end position;
									iStatus |= SPLIT_TIME_STAMP;
								}

								//check if data is splitted across
								if (iStatus & SPLIT_TIME_STAMP) {
									//check to read some more data
									if ((filr.fsize - dwLastseek)
											> (SECTOR_SIZE - iOffset)) {
										//update the seek to next sector
										dwLastseek = dwLastseek + SECTOR_SIZE
												- iOffset;
										//seek
										f_lseek(&filr, dwLastseek);
										fr = f_read(&filr, &dummy, 1, &iIdx); /* dummy read to load the next sector */
										if ((fr == FR_OK) && (iIdx > 0)) {
											pcData = (char*)FatFs.win;	//resuse the buffer maintained by the file system
											//update final lseek for next sample
											//pcSrc1 = strstr(pcData,"$TS");
											pcSrc1 = strstr(pcData, "$");
											if ((pcSrc1)
													&& (pcSrc1
															< &pcData[SECTOR_SIZE])) {
												dwFinalSeek = dwLastseek
														+ (pcSrc1 - pcData); //seek to the next TS
												iIdx = (int32_t)pcSrc1; //end position for second src
											} else {
												dwFinalSeek = filr.fsize; //EOF - update with file size
												iIdx = (int32_t)&pcData[dwFinalSeek
														% SECTOR_SIZE]; //end position for second src
											}
											//first src is in ATresponse buffer, second src is FATFS
											pcSrc1 = pcTmp;
											pcSrc2 = pcData;
										}
									} else {
										//first src is in ATresponse buffer, second src is NULL
										pcSrc1 = pcTmp;
										pcSrc2 = NULL;
										//update lseek
										dwFinalSeek = filr.fsize; //EOF - update with file size
									}
								} else {
									//all data in ATresponse
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = dwLastseek
											+ (iPOSTstatus - (int32_t) pcSrc1);	//seek to the next time stamp
								}
							} else {
								//control should not come here ideally
								//update the seek to skip the bad logged data read
								dwLastseek = dwLastseek + iIdx;
							}
						} else {
							//file system issue ZZZZ
						}
					}
//358072043113601
					if ((fr == FR_OK) && pcTmp) {
						//read so far is successful and time stamp is found
						memset(SampleData, 0, sizeof(SampleData));
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
#if 0
						//strcat(SampleData,"IMEI=358072043113601&ph=8455523642&v=1.20140817.1&sid=0|1|2|3|4&"); //SERIAL
#else
						strcat(SampleData, "IMEI=");
						if (g_pInfoA->cfgIMEI[0] != -1) {
							strcat(SampleData, g_pInfoA->cfgIMEI);
						} else {
							strcat(SampleData, (void*) DEF_IMEI); //be careful as devices with unprogrammed IMEI will need up using same DEF_IMEI
						}
						strcat(SampleData,
								"&ph=8455523642&v=1.20140817.1&sid=0|1|2|3|4&"); //SERIAL
#endif
#else
						strcat(SampleData,"IMEI=358072043113601&ph=8455523642&v=1.20140817.1&sid=0|1|2|3&"); //SERIAL
#endif
						//check if time stamp is split across the two sources
						iOffset = iPOSTstatus - (int32_t) pcTmp; //reuse, iPOSTstatus is end of first src
						if ((iOffset > 0) && (iOffset < TS_SIZE)) {
							//reuse the SampleData tail part to store the complete timestamp
							pcData = &SampleData[SAMPLE_LEN - 1] - TS_SIZE - 1; //to prevent overwrite
							//memset(g_TmpSMScmdBuffer,0,SMS_CMD_LEN);
							//pcData = &g_TmpSMScmdBuffer[0];
							memcpy(pcData, pcTmp, iOffset);
							memcpy(&pcData[iOffset], pcSrc2, TS_SIZE - iOffset);
							pcTmp = pcData;	//point to the copied timestamp
						}

						//format the timestamp
						strcat(SampleData, "sdt=");
						strncat(SampleData, &pcTmp[4], 4);
						strcat(SampleData, "/");
						strncat(SampleData, &pcTmp[8], 2);
						strcat(SampleData, "/");
						strncat(SampleData, &pcTmp[10], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[13], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[16], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[19], 2);
						strcat(SampleData, "&");

						//format the interval
						strcat(SampleData, "i=");
						iOffset = formatfield(pcSrc1, "R", iPOSTstatus, NULL, 0,
								pcSrc2, 7);
						if ((!iOffset) && (pcSrc2)) {
							//format SP from second source
							formatfield(pcSrc2, "R", iIdx, NULL, 0, NULL, 0);
						}
						strcat(SampleData, "&");

						//format the temperature
						strcat(SampleData, "t=");
						iOffset = formatfield(pcSrc1, "A", iPOSTstatus, NULL, 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "A", iIdx, NULL, iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "B", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "B", iIdx, "|", iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "C", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "C", iIdx, "|", iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "D", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "D", iIdx, "|", iOffset, NULL,
									0);
						}
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
						iOffset = formatfield(pcSrc1, "E", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "E", iIdx, "|", iOffset, NULL,
									0);
						}
#endif
						strcat(SampleData, "&");

						strcat(SampleData, "b=");
						iOffset = formatfield(pcSrc1, "F", iPOSTstatus, NULL, 0,
								pcSrc2, 7);
						if (pcSrc2) {
							formatfield(pcSrc2, "F", iIdx, NULL, iOffset, NULL,
									0);
						}
						strcat(SampleData, "&");

						strcat(SampleData, "p=");
						iOffset = formatfield(pcSrc1, "P", iPOSTstatus, NULL, 0,
								pcSrc2, 5);
						if (pcSrc2) {
							formatfield(pcSrc2, "P", iIdx, NULL, iOffset, NULL,
									0);
						}

						//update seek for the next sample
						dwLastseek = dwFinalSeek;
#ifndef CALIBRATION
						if (!(iStatus & TEST_FLAG)) {
							iPOSTstatus = 1;//indicate data is available for POST & SMS
						} else {
							iPOSTstatus = 0;//indicate data is available for POST & SMS
						}
#else
						iPOSTstatus = 0; //no need to send data for POST & SMS for device under calibration
#endif
						g_pInfoB->dwLastSeek = dwLastseek;

						//check if catch is needed due to backlog
						if ((filr.fsize - dwLastseek) > SAMPLE_SIZE) {
							iStatus |= BACKLOG_UPLOAD_ON;
						} else {
							iStatus &= ~BACKLOG_UPLOAD_ON;
						}

					}
					f_close(&filr);
				}
#endif

				if (iPOSTstatus) {
					iPOSTstatus = 0;
					//initialize the RX counters as RX buffer is been used in the aggregrate variables for HTTP POST formation
					RXHeadIdx = RXTailIdx = 0; //ZZZZ end to ensure there is no incoming notification on the modem
					iPOSTstatus = dopost(SampleData);
					if (iPOSTstatus != 0) {
						//redo the post
						uart_tx(
								"AT+CGDCONT=1,\"IP\",\"airtelgprs.com\",\"0.0.0.0\",0,0\r\n"); //APN
						//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
						delay(MODEM_TX_DELAY2);

						uart_tx("AT#SGACT=1,1\r\n");
						delay(MODEM_TX_DELAY2);

						uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
						delay(MODEM_TX_DELAY2);
#ifdef NO_CODE_SIZE_LIMIT
						iPOSTstatus = dopost(SampleData);
						if (iPOSTstatus != 0) {
							//iHTTPRetryFailed++;
							//trigger sms failover
							__no_operation();
						} else {
							//iHTTPRetrySucceeded++;
							__no_operation();
						}
#endif
					}
					//iTimeCnt = 0;
					uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
					delay(MODEM_TX_DELAY2);

					//if upload sms
					delay(5000);		//opt sleep to get http post response
					uploadsms();
					// added for sms retry and file pointer movement..//
					file_pointer_enabled_sms_status = dopost_sms_status();
					if ((file_pointer_enabled_sms_status)
							|| (file_pointer_enabled_gprs_status)) {
						__no_operation();
					} else {
						dwLastseek = dw_file_pointer_back_log;// file pointer moved to original position need to tested.//
						g_pInfoB->dwLastSeek = dwLastseek;
					}

					//save the last read offset
					memcpy(SampleData, g_pInfoB, sizeof(CONFIG_INFOB));
					FRAMCtl_write8((uint8_t*)SampleData, (void*) INFOB_ADDR,
							sizeof(CONFIG_INFOB));

#ifdef POWER_SAVING_ENABLED
					modem_enter_powersave_mode();
#endif
				}
				lcd_show(iDisplayId);//remove the custom print (e.g transmitting)
			}
			P4IE |= BIT1;					// enable interrupt for button input
		}

		if ((iMinuteTick - iSampleTimeElapsed) >= g_iSamplePeriod) {
			iSampleTimeElapsed = iMinuteTick;
			P4IE &= ~BIT1;				// disable interrupt for button input
			//lcd_print_line("Sampling........", LINE2);
			//re-trigger the ADC conversion
			//ADC12CTL0 &= ~ADC12ENC;
			//ADC12CTL0 |= ADC12ENC | ADC12SC;
			sampletemp();
			delay(2000);	//to allow the ADC conversion to complete

			if (iStatus & NETWORK_DOWN) {
				delay(2000);//additional delay to enable ADC conversion to complete
				//check for signal strength
				RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
				uart_tx("AT+CSQ\r\n");
				delay(MODEM_TX_DELAY1);
				memset(ATresponse, 0, sizeof(ATresponse));
				uart_rx(ATCMD_CSQ, ATresponse);
				if (ATresponse[0] != 0) {
					iSignalLevel = strtol(ATresponse, 0, 10);
				}

				if ((iSignalLevel > NETWORK_UP_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					//update the network state
					iStatus &= ~NETWORK_DOWN;
					//send heartbeat
					sendhb();
				}
			}

		}

#ifndef CALIBRATION
		if ((iMinuteTick - iSMSRxPollElapsed) >= SMS_RX_POLL_INTERVAL) {
			iSMSRxPollElapsed = iMinuteTick;
			;
			lcd_print_line("Cfg Processing..", LINE2);
			//sms config reception and processing
#if 1
			dohttpsetup();
			memset(ATresponse, 0, CFG_SIZE);
			doget(ATresponse);
			if (ATresponse[0] == '$')	//check for $
					{
				if (processmsg(ATresponse)) {
					//send heartbeat on successful processing of SMS message
					sendhb();
				}
			} else {
				//no cfg message recevied
				//check signal strength
				iOffset = -1; //reuse to indicate no cfg msg was received
			}
			deactivatehttp();
#endif
			//update the signal strength
			uart_tx("AT+CSQ\r\n");
			delay(2000);
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CSQ, ATresponse);
			if (ATresponse[0] != 0) {

				iSignalLevel = strtol(ATresponse, 0, 10);
			}
			signal_gprs = dopost_gprs_connection_status(GPRS);
			//	if((iSignalLevel <= NETWORK_DOWN_SS) || (iSignalLevel >= NETWORK_MAX_SS)){
			//	signal_gprs=0;
			//}
			//else{
			//		signal_gprs=1;
			//	}

			// adding for network registration report..//

			gprs_network_indication = dopost_gprs_connection_status(GSM);

			//if((iOffset == -1) && ((iSignalLevel <= NETWORK_DOWN_SS) || (iSignalLevel >= NETWORK_MAX_SS)))
			if ((gprs_network_indication == 0)
					|| ((iSignalLevel <= NETWORK_DOWN_SS)
							|| (iSignalLevel >= NETWORK_MAX_SS))) {
				iOffset = 0;
				//network is down
				iStatus |= NETWORK_DOWN;

				//switch the SIM slot;
				pstCfgInfoA = (CONFIG_INFOA*)SampleData;
				memcpy(pstCfgInfoA, (void*) INFOA_ADDR, sizeof(CONFIG_INFOA));
				if (pstCfgInfoA->cfgSIMSlot != 2) //value will be 0xFF in case FRAM was not already populated
						{
					//current sim slot is 1
					//change to sim slot 2
					pstCfgInfoA->cfgSIMSlot = 2;
					lcd_print_line("Switch to SIM 2 ", LINE2);
				} else {
					//current sim slot is 2
					//change to sim slot 1
					pstCfgInfoA->cfgSIMSlot = 1;
					lcd_print_line("Switch to SIM 1 ", LINE2);
				}

				modem_init(pstCfgInfoA->cfgSIMSlot);
				//write to FRAM
				FRAMCtl_write8(pstCfgInfoA, (void*) INFOA_ADDR,
						sizeof(CONFIG_INFOA));

			}

		}
#endif

#ifndef BUZZER_DISABLED
		if (iStatus & BUZZER_ON) {
			iIdx = 10;	//ZZZZ fine-tune for 5 to 10 secs
			while ((iIdx--) && (iStatus & BUZZER_ON)) {
				P3OUT |= BIT4;
				delayus(1);
				P3OUT &= ~BIT4;
				delayus(1);
			}
		}
#endif

		if (((iMinuteTick - iLCDShowElapsed) >= LCD_REFRESH_INTERVAL)
				|| (iLastDisplayId != iDisplayId)) {
			iLastDisplayId = iDisplayId;
			iLCDShowElapsed = iMinuteTick;
			lcd_show(iDisplayId);
		}
		//iTimeCnt++;

#ifndef CALIBRATION
		//process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
		if ((iMinuteTick) && !(iStatus & BACKLOG_UPLOAD_ON)
				&& ((iMinuteTick - iSMSRxPollElapsed)
						< (SMS_RX_POLL_INTERVAL - 2))
				&& ((iMinuteTick - iUploadTimeElapsed) < (g_iUploadPeriod - 2))
				&& ((iMinuteTick - iMsgRxPollElapsed) >= MSG_REFRESH_INTERVAL)) {
			iMsgRxPollElapsed = iMinuteTick;
			//check if messages are available
			RXHeadIdx = RXTailIdx = 0;//ZZZZ reset Rx index to faciliate processing in uart_rx

			uart_tx("AT+CPMS?\r\n");
			delay(1000);
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CPMS, ATresponse);
			if (ATresponse[0] != 0) {
				iIdx = strtol(ATresponse, 0, 10);
				if (iIdx) {
					iIdx = 1;
					lcd_print_line("Msg Processing..", LINE2);
					while (iIdx <= SMS_READ_MAX_MSG_IDX) {
						memset(ATresponse, 0, sizeof(ATresponse));
						recvmsg(iIdx, ATresponse);
						if (ATresponse[0] != 0)	//check for $
								{
							switch (ATresponse[0]) {
								case '1':
									//get temperature values
									memset(SampleData, 0, MSG_RESPONSE_LEN);
									for (iOffset = 0; iOffset < MAX_NUM_SENSORS;
											iOffset++) {
										strcat(SampleData, SensorName[iOffset]);
										strcat(SampleData, "=");
										strcat(SampleData,
												Temperature[iOffset]);
										strcat(SampleData, "C, ");
									}

									// added for show msg//
									strcat(SampleData, "Battery:");
									strcat(SampleData, itoa(iBatteryLevel));
									strcat(SampleData, "%, ");
									if (P4IN & BIT4)	//power not plugged
									{
										strcat(SampleData, "POWER OUT");
									} else if (((P4IN & BIT6))
											&& (iBatteryLevel == 100)) {
										strcat(SampleData, "FULL CHARGE");
									} else {
										strcat(SampleData, "CHARGING");
									}
									iOffset = strlen(SampleData);
									SampleData[iOffset] = 0x1A; //ctrl-Z
									memset(tmpstr, 0, 40); //reuse tmpstr and filler space to form CMGS
									strcat(tmpstr, "AT+CMGS=\"");
									strcat(tmpstr, &ATresponse[6]);
									strcat(tmpstr, "\",129\r\n");
									uart_tx(tmpstr);
									delay(5000);
									uart_tx(SampleData);
									delay(1000);

									break;

								case '2':
									//reset the board by issuing a SW BOR
									PMM_trigBOR();
									break;

								default:
									break;
							}
							//if(processmsg(ATresponse))
							{
								//send heartbeat on successful processing of SMS message
								// sendhb();
							}
						}
						iIdx++;
					}
					delallmsg();
					lcd_show(iDisplayId);
				}
			}
		}

		//low power behavior
		if ((iBatteryLevel <= 10) && (P4IN & BIT4)) {
			lcd_clear();
			delay(1000);
			lcd_print("Low Battery     ");
			delay(5000);
			//reset display
			//PJOUT |= BIT6;							// LCD reset pulled high
			//disable backlight
			lcd_bldisable();
			lcd_off();

			//loop for charger pluging
			while (iBatteryLevel <= 10) {
				if (iLastDisplayId != iDisplayId) {
					iLastDisplayId = iDisplayId;
					//enable backlight
					lcd_blenable();
					//lcd reset
					//lcd_reset();
					//enable lcd
					//lcd_init();
					lcd_on();

					lcd_clear();
					lcd_print("Low Battery     ");
					delay(5000);
					//disable backlight
					lcd_bldisable();
					lcd_off();
					//reset lcd
					//PJOUT |= BIT6;							// LCD reset pulled high
				}

				//power plugged in
				if (!(P4IN & BIT4)) {
					lcd_blenable();
					lcd_on();
					lcd_print("Starting...");
					modem_init(g_pInfoA->cfgSIMSlot);
					iLastDisplayId = iDisplayId = 0;
					lcd_show(iDisplayId);
					break;
				}
			}

		}
#endif
		__no_operation();
		// SET BREAKPOINT HERE
	}
}

void writetoI2C(uint8_t addressI2C, uint8_t dataI2C) {
	return; //void function for the new board with LCD
	/*
	 #ifndef I2C_DISABLED
	 delay(100);
	 i2c_write(SLAVE_ADDR_DISPLAY,addressI2C,1,&dataI2C);
	 #endif
	 */
}

void ConvertADCToTemperature(unsigned int ADCval, char* TemperatureVal,
		int8_t iSensorIdx) {
	float A0V2V, A0R2, A0R1 = 10000, A0V1 = 2.5;
	float A0tempdegC = 0.0;
	int32_t A0tempdegCint = 0;
	volatile int32_t A0R2int = 0;
	int8_t i = 0;
	int8_t iTempIdx = 0;

	A0V2V = 0.00061 * ADCval;		//Converting to voltage. 2.5/4096 = 0.00061

	//NUM = A0V2V*A0R1;
	//DEN = A0V1-A0V2V;

	A0R2 = (A0V2V * A0R1) / (A0V1 - A0V2V);				//R2= (V2*R1)/(V1-V2)

	A0R2int = A0R2;

	//Convert resistance to temperature using Steinhart-Hart algorithm
	A0tempdegC = ConvertoTemp(A0R2int);

	//use calibration formula
#ifndef CALIBRATION
	if ((g_pInfoB->calibration[iSensorIdx][0] > -2.0)
			&& (g_pInfoB->calibration[iSensorIdx][0] < 2.0)
			&& (g_pInfoB->calibration[iSensorIdx][1] > -2.0)
			&& (g_pInfoB->calibration[iSensorIdx][1] < 2.0)) {
		A0tempdegC = A0tempdegC * g_pInfoB->calibration[iSensorIdx][1]
				+ g_pInfoB->calibration[iSensorIdx][0];
	}
#endif
	//Round to one digit after decimal point
	A0tempdegCint = (int32_t) (A0tempdegC * 10);
	//A0tempdegC = A0tempdegC/100;

	if (A0tempdegCint < TEMP_CUTOFF) {
		TemperatureVal[0] = '-';
		TemperatureVal[1] = '-';
		TemperatureVal[2] = '.';
		TemperatureVal[3] = '-';
	} else {
		if (A0tempdegCint < 0) {
			iTempIdx = 1;
			TemperatureVal[0] = '-';
			A0tempdegCint = abs(A0tempdegCint);

		}

		for (i = 2; i >= 0; i--) {
			TemperatureVal[i + iTempIdx] = A0tempdegCint % 10 + 0x30;
			A0tempdegCint = A0tempdegCint / 10;

		}
		TemperatureVal[3 + iTempIdx] = TemperatureVal[2 + iTempIdx];
		//TemperatureVal[0] = (char)results[0];
		//TemperatureVal[1] = (char)results[1];
		TemperatureVal[2 + iTempIdx] = (char) '.';
	}

}

#if 0
//------------------------------------------------------------------------------
// The USCIAB0TX_ISR is structured such that it can be used to transmit any
// number of bytes by pre-loading TXByteCtr with the byte count. Also, TXData
// points to the next byte to transmit.
//------------------------------------------------------------------------------
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_B0_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(USCI_B0_VECTOR)))
#endif
void USCIB0_ISR(void)
{
	switch(__even_in_range(UCB0IV,0x1E))
	{
		case 0x00: break;                      // Vector 0: No interrupts break;
		case 0x02: break;
		case 0x04:
		//resend start if NACK
		EUSCI_B_I2C_masterSendStart(EUSCI_B0_BASE);
		break;// Vector 4: NACKIFG break;
		case 0x18:
		if(TXByteCtr)// Check TX byte counter
		{
			EUSCI_B_I2C_masterMultiByteSendNext(EUSCI_B0_BASE,
					TXData);
			TXByteCtr--;                        // Decrement TX byte counter
		}
		else
		{
			EUSCI_B_I2C_masterMultiByteSendStop(EUSCI_B0_BASE);

			__bic_SR_register_on_exit(CPUOFF);    // Exit LPM0
		}
		break;                                // Vector 26: TXIFG0 break;
		default: break;
	}
}
#endif

float ConvertoTemp(float R) {
	float A1 = 0.00335, B1 = 0.0002565, C1 = 0.0000026059, D1 = 0.00000006329,
			tempdegC;
	int R25 = 9965;

	tempdegC = 1
			/ (A1 + (B1 * log(R / R25)) + (C1 * pow((log(R / R25)), 2))
					+ (D1 * pow((log(R / R25)), 3)));

	tempdegC = tempdegC - 273.15;

	return (float) tempdegC;

}

void parsetime(char* pDatetime, struct tm* pTime) {
	char* pToken1 = NULL;
	char* pToken2 = NULL;
	char* pDelimiter = "\"/,:+-";

	if ((pDatetime) && (pTime)) {
		//string format "yy/MM/dd,hh:mm:ss�zz"
		pToken1 = strtok(pDatetime, pDelimiter);
		if (pToken1) {
			pToken2 = strtok(NULL, pDelimiter);		//skip the first :
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_year = atoi(pToken2) + 2000;	//to get absolute year value
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_mon = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_mday = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_hour = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_min = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_sec = atoi(pToken2);

		}
	}
}

int dopost(char* postdata) {
	int isHTTPResponseAvailable = 0;
	int iRetVal = -1;

	if (iStatus & TEST_FLAG)
		return iRetVal;
#ifndef SAMPLE_POST
	memset(ATresponse, 0, sizeof(ATresponse));
#ifdef NOTFROMFILE
	strcpy(ATresponse,"AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",");
#else
	strcpy(ATresponse, "AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v3/\",");
#endif
	strcat(ATresponse, itoa(strlen(postdata)));
	strcat(ATresponse, ",0\r\n");
//uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
//delay(5000);

	isHTTPResponseAvailable = 0;
//uart_tx("AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",383,0\r\n");
	uart_tx(ATresponse);
	delay(10000);		//opt
	iHTTPRespDelayCnt = 0;
	while ((!isHTTPResponseAvailable)
			&& iHTTPRespDelayCnt <= HTTP_RESPONSE_RETRY) {
		delay(1000);
		iHTTPRespDelayCnt++;
		uart_rx(ATCMD_HTTPSND, ATresponse);
		if (ATresponse[0] == '>') {
			isHTTPResponseAvailable = 1;
		}
	}

	if (isHTTPResponseAvailable) {
		file_pointer_enabled_gprs_status = 1; // added for gprs status for file pointer movement///
		uart_tx(postdata);
#if 0
		//testing split http post
		memset(filler,0,sizeof(filler));
		memcpy(filler, postdata, 200);
		uart_tx(filler);
		delay(10000);
		memset(filler,0,sizeof(filler));
		memcpy(filler, &postdata[200], iLen - 200);
		uart_tx(filler);
#endif
		//delay(10000);
		iRetVal = 0;
		iPostSuccess++;
	} else {
		file_pointer_enabled_gprs_status = 0; // added for gprs status for file pointer movement..////
		iPostFail++;
	}
#else
	uart_tx("AT#HTTPCFG=1,\"67.205.14.22\",80,0,,,0,120,1\r\n");
	delay(5000);

//uart_tx("AT#HTTPCFG=1,\"54.175.219.8\",80,0,,,0,120,1\r\n");
//delay(5000);

//uart_tx("AT#HTTPCFG=0,\"106.10.138.240\",80,0,,,0,120,1\r\n"); //http profile 0
//delay(5000);
//uart_tx("AT#HTTPCFG=0,\"www.yahoo.com\",80,0,,,0,120,1\r\n"); //http profile 0
//delay(5000);

	uart_tx("AT#HTTPSND=1,0,\"/post.php?dir=galcore&dump\",26,1\r\n");
	isHTTPResponseAvailable = 0;
	iIdx = 0;
	while ((!isHTTPResponseAvailable) && iIdx <= HTTP_RESPONSE_RETRY)
	{
		delay(1000);
		iIdx++;
		uart_rx(ATCMD_HTTPSND,ATresponse);
		if(ATresponse[0] == '>')
		{
			isHTTPResponseAvailable = 1;
		}
	}

	if(isHTTPResponseAvailable)
	{
		uart_tx("imessy=martina&mary=johyyy");
		delay(10000);
	}
#endif
	return iRetVal;
}

int dopost_sms_status(void) {
	char l_file_pointer_enabled_sms_status = 0;
	int isHTTPResponseAvailable = 0;
	int iRetVal = -1;
	int i = 0, j = 0;

	if (iStatus & TEST_FLAG)
		return iRetVal;
	iHTTPRespDelayCnt = 0;
	while ((!isHTTPResponseAvailable)
			&& iHTTPRespDelayCnt <= HTTP_RESPONSE_RETRY) {
		delay(1000);
		iHTTPRespDelayCnt++;
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RX[i] == '+') && (RX[j] == 'C') && (RX[j + 1] == 'M')
					&& (RX[j + 2] == 'G') && (RX[j + 3] == 'S')
					&& (RX[j + 4] == ':')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
	}
	if (isHTTPResponseAvailable == 0) {
		l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
	}
	return l_file_pointer_enabled_sms_status;
}

int dopost_gprs_connection_status(char status) {
	char l_file_pointer_enabled_sms_status = 0;
	int isHTTPResponseAvailable = 0;
	int iRetVal = -1;
	int i = 0, j = 0;

	if (iStatus & TEST_FLAG)
		return iRetVal;
	iHTTPRespDelayCnt = 0;
	if (status == GSM) {
		uart_tx("AT+CREG?\r\n");
		delay(MODEM_TX_DELAY1);
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RX[i] == '+') && (RX[j] == 'C') && (RX[j + 1] == 'R')
					&& (RX[j + 2] == 'E') && (RX[j + 3] == 'G')
					&& (RX[j + 4] == ':') && (RX[j + 8] == '1')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
		if (isHTTPResponseAvailable == 0) {
			l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
		}
	}
//////
	if (status == GPRS) {
		uart_tx("AT+CGREG?\r\n");
		delay(MODEM_TX_DELAY1);
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RX[i] == '+') && (RX[j] == 'C') && (RX[j + 1] == 'G')
					&& (RX[j + 2] == 'R') && (RX[j + 3] == 'E')
					&& (RX[j + 4] == 'G') && (RX[j + 5] == ':')
					&& (RX[j + 7] == '0') && (RX[j + 8] == ',')
					&& (RX[j + 9] == '1')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
		if (isHTTPResponseAvailable == 0) {
			uart_tx("AT+CGATT=1\r\n");
			delay(MODEM_TX_DELAY1);
			l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
		}
	}

	return l_file_pointer_enabled_sms_status;

}

char* itoa(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(tmpstr, 0, sizeof(tmpstr));

	if (num == 0) {
		tmpstr[0] = 0x30;
		tmpstr[1] = 0x30;
	} else {
		while (num != 0) {
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num / 10;
		}

		//pad with zero if single digit
		if (iIdx == 1) {
			tmpstr[0] = 0x30; //leading 0
			tmpstr[1] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				tmpstr[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return tmpstr;
}

char* itoa_nopadding(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(tmpstr, 0, sizeof(tmpstr));

	if (num == 0) {
		tmpstr[0] = 0x30;
		//tmpstr[1] = 0x30;
	} else {
		while (num != 0) {
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num / 10;
		}

		//pad with zero if single digit
		if (iIdx == 1) {
			//tmpstr[0] = 0x30; //leading 0
			tmpstr[0] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				tmpstr[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return tmpstr;
}

FRESULT logsampletofile(FIL* fobj, int* tbw) {
	int bw = 0;	//bytes written

	if (!(iStatus & TEST_FLAG)) {
		fr = f_open(fobj, FILE_NAME, FA_WRITE | FA_OPEN_ALWAYS);
		if (fr == FR_OK) {
			if (fobj->fsize) {
				//append to the file
				f_lseek(fobj, fobj->fsize);
			}
		}

		if (iStatus & LOG_TIME_STAMP) {
			rtc_get(&currTime);

#if 1
			memset(acLogData, 0, sizeof(acLogData));
			strcat(acLogData, "$TS=");
			strcat(acLogData, itoa(currTime.tm_year));
			strcat(acLogData, itoa(currTime.tm_mon));
			strcat(acLogData, itoa(currTime.tm_mday));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_hour));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_min));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_sec));
			strcat(acLogData, ",");
			strcat(acLogData, "R");	//removed =
			strcat(acLogData, itoa(g_iSamplePeriod));
			strcat(acLogData, ",");
			strcat(acLogData, "\n");
			fr = f_write(fobj, acLogData, strlen(acLogData), &bw);
#else
			bw = f_printf(fobj,"$TS=%04d%02d%02d:%02d:%02d:%02d,R=%d,", currTime.tm_year, currTime.tm_mon, currTime.tm_mday,
					currTime.tm_hour, currTime.tm_min, currTime.tm_sec,g_iSamplePeriod);
#endif
			if (bw > 0) {
				*tbw += bw;
				iStatus &= ~LOG_TIME_STAMP;
			}

			if (currTime.tm_mday != g_iCurrDay) {
				//ZZZZ day has changed, update the filename
			}
		}

		//get battery level
#ifndef BATTERY_DISABLED
		iBatteryLevel = batt_getlevel();
#else
		iBatteryLevel = 0;
#endif
		//log sample period, battery level, power plugged, temperature values
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
		//bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,E=%s,", iBatteryLevel,
		//			  !(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3],Temperature[4]);
		memset(acLogData, 0, sizeof(acLogData));
		strcat(acLogData, "F");
		strcat(acLogData, itoa(iBatteryLevel));
		strcat(acLogData, ",");
		strcat(acLogData, "P");
		if (P4IN & BIT4) {
			strcat(acLogData, "0,");
		} else {
			strcat(acLogData, "1,");
		}
		strcat(acLogData, "A");
		strcat(acLogData, Temperature[0]);
		strcat(acLogData, ",");
		strcat(acLogData, "B");
		strcat(acLogData, Temperature[1]);
		strcat(acLogData, ",");
		strcat(acLogData, "C");
		strcat(acLogData, Temperature[2]);
		strcat(acLogData, ",");
		strcat(acLogData, "D");
		strcat(acLogData, Temperature[3]);
		strcat(acLogData, ",");
		strcat(acLogData, "E");
		strcat(acLogData, Temperature[4]);
		strcat(acLogData, ",");
		strcat(acLogData, "\n");
		fr = f_write(fobj, acLogData, strlen(acLogData), &bw);
#else
		bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,", iBatteryLevel,
				!(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3]);
#endif

		if (bw > 0) {
			*tbw += bw;
		}

		f_sync(fobj);
		return f_close(fobj);
	} else {
		return FR_OK;
	}
}

int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize) {
	char* pcTmp = NULL;
	char* pcExt = NULL;
	int16_t iSampleCnt = 0;
	int16_t ret = 0;
	int8_t iFlag = 0;

	//pcTmp = strstr(pcSrc,fieldstr);
	pcTmp = strchr(pcSrc, fieldstr[0]);
	iFlag = iFlagVal;
	while ((pcTmp && !lastoffset)
			|| (pcTmp && lastoffset && (pcTmp < (char*)lastoffset))) {
		iSampleCnt = lastoffset - (int32_t) pcTmp;
		if ((iSampleCnt > 0) && (iSampleCnt < iFieldSize)) {
			//the field is splitted across
			//reuse the SampleData tail part to store the complete timestamp
			pcExt = &SampleData[SAMPLE_LEN - 1] - iFieldSize - 1; //to accommodate field pair and null termination
			//memset(g_TmpSMScmdBuffer,0,SMS_CMD_LEN);
			//pcExt = &g_TmpSMScmdBuffer[0];
			memset(pcExt, 0, iFieldSize + 1);
			memcpy(pcExt, pcTmp, iSampleCnt);
			memcpy(&pcExt[iSampleCnt], pcExtSrc, iFieldSize - iSampleCnt);
			pcTmp = pcExt;	//point to the copied field
		}

		iSampleCnt = 0;
		while (pcTmp[FORMAT_FIELD_OFF + iSampleCnt] != ',')	//ZZZZ recheck whenever the log formatting changes
		{
			iSampleCnt++;
		}
		if (iFlag) {
			//non first instance
			strcat(SampleData, ",");
			strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
		} else {
			//first instance
			if (seperator) {
				strcat(SampleData, seperator);
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			} else {
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			}
		}
		//pcTmp = strstr(&pcTmp[3],fieldstr);
		pcTmp = strchr(&pcTmp[FORMAT_FIELD_OFF], fieldstr[0]);
		iFlag++;
		ret = 1;
	}

	return ret;
}

#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode()
{
	uint8_t iRetVal = 0;

	delay(15000);		//to allow the last transmit to do successfully
//set DTR OFF (high)
	P3OUT |= BIT3;//DTR
	delay(100);//opt
//get CTS to confirm the modem entered power saving mode
	if(P3IN & BIT7)//CTS
	{
		//CTS OFF (high)
		iRetVal = 1;
	}
	return iRetVal;
}

int8_t modem_exit_powersave_mode()
{
	uint16_t iRetVal = 0;

//set DTR ON (low)
	P3OUT &= ~BIT3;//DTR
	delay(100);//opt
//get CTS to confirm the modem exit power saving mode
	iRetVal = P3IN;
	if(!(iRetVal & BIT7))//CTS
	{
		//CTS ON (low)
		iRetVal = 1;
	}

//delay(15000);
	return iRetVal;

}

#endif

void validatealarmthreshold() {

	CONFIG_INFOA* pstCfgInfoA = (CONFIG_INFOA*) INFOA_ADDR;
	int8_t iCnt = 0;

	for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
		//validate low temp threshold
		if ((pstCfgInfoA->stTempAlertParams[iCnt].threshcold < MIN_TEMP)
				|| (pstCfgInfoA->stTempAlertParams[iCnt].threshcold > MAX_TEMP)) {
			pstCfgInfoA->stTempAlertParams[iCnt].threshcold =
					LOW_TEMP_THRESHOLD;
		}

		if ((pstCfgInfoA->stTempAlertParams[iCnt].mincold
				< MIN_CNF_TEMP_THRESHOLD)
				|| (pstCfgInfoA->stTempAlertParams[iCnt].mincold
						> MAX_CNF_TEMP_THRESHOLD)) {
			pstCfgInfoA->stTempAlertParams[iCnt].mincold =
					ALARM_LOW_TEMP_PERIOD;
		}

		//validate high temp threshold
		if ((pstCfgInfoA->stTempAlertParams[iCnt].threshhot < MIN_TEMP)
				|| (pstCfgInfoA->stTempAlertParams[iCnt].threshhot > MAX_TEMP)) {
			pstCfgInfoA->stTempAlertParams[iCnt].threshhot =
					HIGH_TEMP_THRESHOLD;
		}
		if ((pstCfgInfoA->stTempAlertParams[iCnt].minhot
				< MIN_CNF_TEMP_THRESHOLD)
				|| (pstCfgInfoA->stTempAlertParams[iCnt].minhot
						> MAX_CNF_TEMP_THRESHOLD)) {
			pstCfgInfoA->stTempAlertParams[iCnt].minhot =
					ALARM_HIGH_TEMP_PERIOD;
		}

	}

}

void uploadsms() {
	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	int iIdx = 0;
	int iPOSTstatus = 0;
	char sensorId[2] = { 0, 0 };

	iPOSTstatus = strlen(SampleData);

	memset(sensorId, 0, sizeof(sensorId));
	memset(ATresponse, 0, sizeof(ATresponse));
	strcat(ATresponse, SMS_DATA_MSG_TYPE);

	pcSrc1 = strstr(SampleData, "sdt=");	//start of TS
	pcSrc2 = strstr(pcSrc1, "&");	//end of TS
	pcTmp = strtok(&pcSrc1[4], "/:");

	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(ATresponse, pcTmp);	//get year
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, pcTmp);	//get month
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(ATresponse, pcTmp);	//get day
				strcat(ATresponse, ":");
			}
		}

		//fetch time
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, pcTmp);	//get hour
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(ATresponse, pcTmp);	//get minute
				pcTmp = strtok(NULL, "&");
				if ((pcTmp) && (pcTmp < pcSrc2)) {
					strcat(ATresponse, pcTmp);	//get sec
					strcat(ATresponse, ",");
				}
			}
		}
	}

	pcSrc1 = strstr(pcSrc2 + 1, "i=");	//start of interval
	pcSrc2 = strstr(pcSrc1, "&");	//end of interval
	pcTmp = strtok(&pcSrc1[2], "&");
	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(ATresponse, pcTmp);
		strcat(ATresponse, ",");
	}

	pcSrc1 = strstr(pcSrc2 + 1, "t=");	//start of temperature
	pcData = strstr(pcSrc1, "&");	//end of temperature
	pcSrc2 = strstr(pcSrc1, ",");
	if ((pcSrc2) && (pcSrc2 < pcData)) {
		iIdx = 0xA5A5;	//temperature has series values
	} else {
		iIdx = 0xDEAD;	//temperature has single value
	}

//check if temperature values are in series
	if (iIdx == 0xDEAD) {
		//pcSrc1 is t=23.1|24.1|25.1|26.1|28.1
		pcTmp = strtok(&pcSrc1[2], "|&");
		for (iIdx = 0; (iIdx < MAX_NUM_SENSORS) && (pcTmp); iIdx++) {
			sensorId[0] = iIdx + 0x30;
			strcat(ATresponse, sensorId);	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc2 = &ATresponse[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc2);
				strcat(ATresponse, pcSrc2);	//add encoded temp value
			} else {
				strcat(ATresponse, "/W");	//add encoded temp value
			}
			strcat(ATresponse, ",");

			pcTmp = strtok(NULL, "|&");

		}
	} else {
		iIdx = 0;
		pcSrc2 = strstr(pcSrc1, "|");	//end of first sensor
		if (!pcSrc2)
			pcSrc2 = pcData;
		pcTmp = strtok(&pcSrc1[2], ",|&");  //get first temp value

		while ((pcTmp) && (pcTmp < pcSrc2)) {
			sensorId[0] = iIdx + 0x30;
			strcat(ATresponse, sensorId);	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc1);
				strcat(ATresponse, pcSrc1);	//add encoded temp value
			} else {
				strcat(ATresponse, "/W");	//add encoded temp value
			}

			pcTmp = strtok(NULL, ",|&");
			while ((pcTmp) && (pcTmp < pcSrc2)) {
				//encode the temp value
				memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
				//check temp is --.- in case of sensor plugged out
				if (pcTmp[1] != '-') {
					pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
					encode(strtod(pcTmp, NULL), pcSrc1);
					strcat(ATresponse, pcSrc1);	//add encoded temp value
				} else {
					strcat(ATresponse, "/W");	//add encoded temp value
				}
				pcTmp = strtok(NULL, ",|&");

			}

			//check if we can start with next temp series
			if ((pcTmp) && (pcTmp < pcData)) {
				strcat(ATresponse, ",");
				iIdx++;	//start with next sensor
				pcSrc2 = strstr(&pcTmp[strlen(pcTmp) + 1], "|"); //adjust the last postion to next sensor end
				if (!pcSrc2) {
					//no more temperature series available
					pcSrc2 = pcData;
				}
			}

		}
	}

	pcSrc1 = pcTmp + strlen(pcTmp) + 1;	//pcTmp is b=yyy
//check if battery has series values
	if (*pcSrc1 != 'p') {
		pcSrc2 = strstr(pcSrc1, "&");	//end of battery
		pcTmp = strtok(pcSrc1, "&");	//get all series values except the first
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, ",");
			pcSrc1 = strrchr(pcTmp, ','); //postion to the last battery level  (e.g pcTmp 100 or 100,100,..
			if (pcSrc1) {
				strcat(ATresponse, pcSrc1 + 1); //past the last comma
			} else {
				strcat(ATresponse, pcTmp); //no comma
			}
			strcat(ATresponse, ",");

			pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
			if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
				strcat(ATresponse, pcSrc1 + 1);
			} else {
				//power plugged does not have series values
				pcTmp = pcSrc2 + 1;
				strcat(ATresponse, &pcTmp[2]); //pcTmp contains p=yyy
			}

		}

	} else {
		//battery does not have series values
		//strcat(ATresponse,",");
		strcat(ATresponse, &pcTmp[2]); //pcTmp contains b=yyy

		strcat(ATresponse, ",");
		pcSrc2 = pcTmp + strlen(pcTmp);  //end of battery
		pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
		if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
			strcat(ATresponse, pcSrc1 + 1);	//last power plugged values is followed by null
		} else {
			//power plugged does not have series values
			pcTmp = pcSrc2 + 1;
			strcat(ATresponse, &pcTmp[2]); //pcTmp contains p=yyy
		}

	}
	iIdx = strlen(ATresponse);
	ATresponse[iIdx] = 0x1A;

	sendmsg(ATresponse);

}

int8_t processmsg(char* pSMSmsg) {
	char* pcTmp = NULL;
	int8_t iCnt = 0;
	int8_t iCurrSeq = 0;
	CONFIG_INFOA* pstCfgInfoA = NULL;

	pcTmp = strtok(pSMSmsg, ",");

	while ((pcTmp) && (iCurrSeq != -1)) {
		if (pcTmp) {
			//get msg type
			switch (pcTmp[3]) {
				case '1':
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						pstCfgInfoA = (CONFIG_INFOA*)SampleData;
						memcpy(pstCfgInfoA, (void*) INFOA_ADDR,
								sizeof(CONFIG_INFOA));
						//get & set gateway
						//uart_tx("AT+CSCA=\"");
						//uart_tx(pcTmp);
						//uart_tx("\",145\r\n");
						//delay(1000);
						strncpy(pstCfgInfoA->cfgGW, pcTmp, strlen(pcTmp));

						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							//get upload mode
							pstCfgInfoA->cfgUploadMode = strtol(pcTmp, 0, 10);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								//get APN
								memcpy(pstCfgInfoA->cfgAPN, pcTmp,
										strlen(pcTmp));
								pcTmp = strtok(NULL, ",");
								if (pcTmp) {
									//get upload interval
									g_iUploadPeriod = strtol(pcTmp, 0, 10);
									pcTmp = strtok(NULL, ",");
									if (pcTmp) {
										g_iSamplePeriod = strtol(pcTmp, 0, 10);
										pcTmp = strtok(NULL, ",");
									}
								}
							}
						}

						if (pcTmp) {
							if (strtol(pcTmp, 0, 10)) {
								iStatus |= RESET_ALERT;
								//set buzzer OFF
								iStatus &= ~BUZZER_ON;
								//reset alarm state and counters
								for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
									//reset the alarm
									TEMP_ALARM_CLR(iCnt);
									//initialize confirmation counter
									g_iAlarmCnfCnt[iCnt] = 0;
								}
							} else {
								iStatus &= ~RESET_ALERT;
							}
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								if (strtol(pcTmp, 0, 10)) {
									iStatus |= ENABLE_SECOND_SLOT;
									pstCfgInfoA->cfgSIMSlot = 2;
								} else {
									iStatus &= ~ENABLE_SECOND_SLOT;
									pstCfgInfoA->cfgSIMSlot = 1;
								}
								if (pstCfgInfoA->cfgSIMSlot
										!= g_pInfoA->cfgSIMSlot) {
									//sim slots need to be switched
									modem_init(pstCfgInfoA->cfgSIMSlot);
								}
							}

							FRAMCtl_write8(pstCfgInfoA, (void*) INFOA_ADDR,
									sizeof(CONFIG_INFOA));
							iCnt = 1;
						}
					}
					break;
				case '2':
					pcTmp = strtok(NULL, ",");
#if 1
					if (pcTmp) {
						iCurrSeq = strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
					}

					if (iCurrSeq != g_iLastCfgSeq) {
						g_iLastCfgSeq = iCurrSeq;
#else
						if(pcTmp)
						{
#endif
						pstCfgInfoA = (CONFIG_INFOA*)SampleData;
						memcpy(pstCfgInfoA, (void*) INFOA_ADDR,
								sizeof(CONFIG_INFOA));

						for (iCnt = 0; (iCnt < MAX_NUM_SENSORS) && (pcTmp);
								iCnt++) {
							pstCfgInfoA->stTempAlertParams[iCnt].mincold =
									strtol(pcTmp, 0, 10);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								pstCfgInfoA->stTempAlertParams[iCnt].threshcold =
										strtod(pcTmp, NULL);
								pcTmp = strtok(NULL, ",");
								if (pcTmp) {
									pstCfgInfoA->stTempAlertParams[iCnt].minhot =
											strtol(pcTmp, 0, 10);
									pcTmp = strtok(NULL, ",");
									if (pcTmp) {
										pstCfgInfoA->stTempAlertParams[iCnt].threshhot =
												strtod(pcTmp, NULL);
										pcTmp = strtok(NULL, ",");
									}
								}
							}
						}
						if (pcTmp) {
							pstCfgInfoA->stBattPowerAlertParam.minutespower =
									strtol(pcTmp, 0, 10);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								pstCfgInfoA->stBattPowerAlertParam.enablepoweralert =
										strtol(pcTmp, 0, 10);
								pcTmp = strtok(NULL, ",");
								if (pcTmp) {
									pstCfgInfoA->stBattPowerAlertParam.minutesbathresh =
											strtol(pcTmp, 0, 10);
									pcTmp = strtok(NULL, ",");
									if (pcTmp) {
										pstCfgInfoA->stBattPowerAlertParam.battthreshold =
												strtol(pcTmp, 0, 10);
									}
								}
							}

						} else {
							pstCfgInfoA->stBattPowerAlertParam.enablepoweralert =
									0;
							pstCfgInfoA->stBattPowerAlertParam.battthreshold =
									101;
						}
						FRAMCtl_write8(pstCfgInfoA, (void*) INFOA_ADDR,
								sizeof(CONFIG_INFOA));
						iCnt = 1;
					} else {
						iCurrSeq = -1; //stop further processing as cfg is identical
					}
					break;
				case '3':
					iCnt = 0;
#if 0
					//defunct SMS destination number configuration
					pcTmp = strtok(NULL,",");
					if(pcTmp)
					{
						pstCfgInfoB = SampleData;
						memcpy(pstCfgInfoB,INFOB_ADDR,sizeof(CONFIG_INFOB));
						pstCfgInfoB->iDAcount=0;
						for(iCnt; (iCnt < MAX_SMS_NUM) && (pcTmp); iCnt++)
						{
							memcpy(pstCfgInfoB->cfgSMSDA[iCnt],pcTmp,strlen(pcTmp));
							pcTmp = strtok(NULL,",");
						}
						pstCfgInfoB->iDAcount = iCnt;
						FRAMCtl_write8(pstCfgInfoB,INFOB_ADDR,sizeof(CONFIG_INFOB));
					}
#endif
					break;
				case '4':
					break;
			}
			pcTmp = strtok(NULL, ","); //get next mesg
		}
	}
	return iCnt;
}

void sendhb() {
	char* pcTmp = NULL;

//send heart beat
	memset(SampleData, 0, sizeof(SampleData));
	strcat(SampleData, SMS_HB_MSG_TYPE);
#if 0
	strcat(SampleData,"358072043113601");	//ZZZZ remove hardcoding //SERIAL
	strcat(SampleData,",");
#else
	strcat(SampleData, g_pInfoA->cfgIMEI);
	strcat(SampleData, ",");
	if (g_pInfoA->cfgSIMSlot == 2) {
		strcat(SampleData, "1,");
	} else {
		strcat(SampleData, "0,");
	}
#endif
	strcat(SampleData, DEF_GW);		//ZZZZ read from INFOA
	strcat(SampleData, ",");
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	strcat(SampleData, "1,1,1,1,1,");//ZZZZ to be changed based on jack detection
#else
			strcat(SampleData,"1,1,1,1,");	//ZZZZ to be changed based on jack detection
#endif

	pcTmp = itoa(batt_getlevel());	//opt by directly using tmpstr
	strcat(SampleData, pcTmp);
	if (P4IN & BIT4) {
		strcat(SampleData, ",0");
	} else {
		strcat(SampleData, ",1");
	}

	SampleData[strlen(SampleData)] = 0x1A;
	sendmsg(SampleData);
}

void lcd_init() {
	memset(SampleData, 0, LCD_INIT_PARAM_SIZE);

	SampleData[0] = 0x38;
	SampleData[1] = 0x39;
	SampleData[2] = 0x14;
	SampleData[3] = 0x78;
	SampleData[4] = 0x5E;
	SampleData[5] = 0x6D;
	SampleData[6] = 0x0C;
	SampleData[7] = 0x01;
	SampleData[8] = 0x06;

	i2c_write(0x3e, 0, LCD_INIT_PARAM_SIZE, (uint8_t*)SampleData);
#if 0
	delay(1000);

	SampleData[0] = 0x41;
	SampleData[1] = 0x42;
	i2c_write(0x3e,0x40,2,SampleData);
	delay(100);
#endif
}

void lcd_show(int8_t iItemId) {
	int iIdx = 0;
	int iCnt = 0;
	float signal_strength = 0;
	float local_signal = 0;

//check if there is a change in display id
//if(iLastDisplayId != iItemId) lcd_clear();
	lcd_clear();

	memset(SampleData, 0, LCD_DISPLAY_LEN);
//get local time
	rtc_getlocal(&currTime);
	strcat(SampleData, itoa(currTime.tm_year));
	strcat(SampleData, "/");
	strcat(SampleData, itoa(currTime.tm_mon));
	strcat(SampleData, "/");
	strcat(SampleData, itoa(currTime.tm_mday));
	strcat(SampleData, " ");

	strcat(SampleData, itoa(currTime.tm_hour));
	strcat(SampleData, ":");
	strcat(SampleData, itoa(currTime.tm_min));
	iIdx = strlen(SampleData); //marker

	switch (iItemId) {
		case 0:
			memset(&Temperature[1], 0, TEMP_DATA_LEN + 1); //initialize as it will be used as scratchpad during POST formatting
			ConvertADCToTemperature(ADCvar[1], &Temperature[1], 1);
			strcat(SampleData, Temperature[1]);
			strcat(SampleData, "C ");
			strcat(SampleData, itoa(iBatteryLevel));
			strcat(SampleData, "% ");
			//  if(signal_gprs==1){
			if ((iSignalLevel > NETWORK_DOWN_SS)
					&& (iSignalLevel < NETWORK_MAX_SS)) {
				//	  strcat(SampleData,"G:");
				if (signal_gprs == 1) {
					strcat(SampleData, "G");
				} else {
					strcat(SampleData, "S");
				}
				local_signal = iSignalLevel;
				local_signal = (((local_signal - NETWORK_ZERO)
						/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
				strcat(SampleData, itoa(local_signal));
				strcat(SampleData, "%");
			} else {
				strcat(SampleData, "S --  ");
			}

			//strcat(SampleData,itoa(iSignalLevel));strcat(SampleData,"S");
#if 0
			if(iSignalLevel == 99)
			{
				strcat(SampleData,"S:--");
			}
			else if((iSignalLevel >= LOW_RANGE_MIN) && (iSignalLevel <= LOW_RANGE_MAX))
			{
				strcat(SampleData,"S:LO");
			}
			else if((iSignalLevel >= MED_RANGE_MIN) && (iSignalLevel <= MED_RANGE_MAX))
			{
				strcat(SampleData,"S:MD");
			}
			else if((iSignalLevel >= HIGH_RANGE_MIN) && (iSignalLevel <= HIGH_RANGE_MAX))
			{
				strcat(SampleData,"S:HI");
			}
#endif
			iCnt = 0xff;
			break;

		case 1:
			iCnt = 0;
			break;
		case 2:
			iCnt = 1;
			break;
		case 3:
			iCnt = 2;
			break;
		case 4:
			iCnt = 3;
			break;
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
		case 5:
			iCnt = 4;
			break;
		case 6:
			iCnt = 0xff;
#else
			case 5: iCnt = 0xff;
#endif

			strcat(SampleData, itoa(iBatteryLevel));
			strcat(SampleData, "% ");
			if (TEMP_ALARM_GET(MAX_NUM_SENSORS) == TEMP_ALERT_CNF) {
				strcat(SampleData, "BATT ALERT");
			} else if (P4IN & BIT4)	//power not plugged
			{
				strcat(SampleData, "POWER OUT");
			} else if (((P4IN & BIT6)) && (iBatteryLevel == 100)) {
				strcat(SampleData, "FULL CHARGE");
			} else {
				strcat(SampleData, "CHARGING");
			}

			break;
			// added for new display//
		case 7:
			iCnt = 0xff;
			strcat(SampleData, "SIM1 ");	//current sim slot is 1
			if (g_pInfoA->cfgSIMSlot != 1) {
				strcat(SampleData, "  --  ");
			} else {
				if ((iSignalLevel > NETWORK_DOWN_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					local_signal = iSignalLevel;
					local_signal = (((local_signal - NETWORK_ZERO)
							/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
					strcat(SampleData, itoa(local_signal));
					strcat(SampleData, "% ");
					if (signal_gprs == 1) {
						strcat(SampleData, "G:YES");
					} else {
						strcat(SampleData, "G:NO");
					}
				} else {
					strcat(SampleData, "  --  ");
				}
			}
			break;
		case 8:
			iCnt = 0xff;
			strcat(SampleData, "SIM2 ");	//current sim slot is 2
			if (g_pInfoA->cfgSIMSlot != 2) {
				strcat(SampleData, "  --  ");
			} else {
				if ((iSignalLevel > NETWORK_DOWN_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					local_signal = iSignalLevel;
					local_signal = (((local_signal - NETWORK_ZERO)
							/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
					strcat(SampleData, itoa(local_signal));
					strcat(SampleData, "% ");
					if (signal_gprs == 1) {
						strcat(SampleData, "G:YES");
					} else {
						strcat(SampleData, "G:NO");
					}
				} else {
					strcat(SampleData, "  --  ");
				}
			}
			break;
		default:
			break;
	}

	if (iCnt != 0xff) {
		memset(&Temperature[iCnt], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
		ConvertADCToTemperature(ADCvar[iCnt], &Temperature[iCnt], iCnt);

		if (TEMP_ALARM_GET(iCnt) == TEMP_ALERT_CNF) {
			strcat(SampleData, "ALERT ");
			strcat(SampleData, SensorName[iCnt]);
			strcat(SampleData, " ");
			strcat(SampleData, Temperature[iCnt]);
			strcat(SampleData, "C ");
		} else {
			strcat(SampleData, "Sensor ");
			strcat(SampleData, SensorName[iCnt]);
			strcat(SampleData, " ");
			strcat(SampleData, Temperature[iCnt]);
			strcat(SampleData, "C ");
		}
	}

//display the lines
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, (uint8_t*)SampleData);
	delay(100);
	lcd_setaddr(0x40);	//go to next line
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, &SampleData[iIdx]);

}

void lcd_print(char* pcData) {
	int8_t len = strlen(pcData);

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	lcd_clear();
	i2c_write(0x3e, 0x40, len, (uint8_t*)pcData);
}

void lcd_print_line(char* pcData, int8_t iLine) {
	int8_t len = strlen(pcData);

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	if (iLine == 2) {
		lcd_setaddr(0x40);
	} else {
		lcd_setaddr(0x0);
	}
	i2c_write(0x3e, 0x40, len, (uint8_t*)pcData);
}

void lcd_reset() {
	PJOUT &= ~BIT6;
	delay(100);	//delay 100 ms
	PJOUT |= BIT6;
}

void lcd_blenable() {
	PJOUT |= BIT7;
}

void lcd_bldisable() {
	PJOUT &= ~BIT7;
}

void lcd_clear() {
	SampleData[0] = 0x01;
	i2c_write(0x3e, 0, 1, (uint8_t*)SampleData);
	delay(100);
}

void lcd_on() {
	SampleData[0] = 0x0C;
	i2c_write(0x3e, 0, 1, (uint8_t*)SampleData);
	delay(100);
}

void lcd_off() {
	SampleData[0] = 0x08;
	i2c_write(0x3e, 0, 1, (uint8_t*)SampleData);
	delay(100);
}

void lcd_setaddr(int8_t addr) {
	SampleData[0] = addr | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t*)SampleData);
	delay(100);
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG)
)
  {
    case ADC12IV_NONE:        break;        // Vector  0:  No interrupt
    case ADC12IV_ADC12OVIFG:  break;        // Vector  2:  ADC12MEMx Overflow
    case ADC12IV_ADC12TOVIFG: break;        // Vector  4:  Conversion time overflow
    case ADC12IV_ADC12HIIFG:  break;        // Vector  6:  ADC12BHI
    case ADC12IV_ADC12LOIFG:  break;        // Vector  8:  ADC12BLO
    case ADC12IV_ADC12INIFG:  break;        // Vector 10:  ADC12BIN
    case ADC12IV_ADC12IFG0:                 // Vector 12:  ADC12MEM0 Interrupt
      if (ADC12MEM0 >= 0x7ff)               // ADC12MEM0 = A1 > 0.5AVcc?
        P1OUT |= BIT0;                      // P1.0 = 1
      else
        P1OUT &= ~BIT0;                     // P1.0 = 0
      __bic_SR_register_on_exit(LPM0_bits); // Exit active CPU
      break;                                // Clear CPUOFF bit from 0(SR)
    case ADC12IV_ADC12IFG1:   break;        // Vector 14:  ADC12MEM1
    case ADC12IV_ADC12IFG2:   		        // Vector 16:  ADC12MEM2
    	//ADCvar[0] = ADC12MEM2;                     // Read conversion result
    	ADCvar[0] += ADC12MEM2;                     // Read conversion result
    	break;
    case ADC12IV_ADC12IFG3:   		        // Vector 18:  ADC12MEM3
    	//ADCvar[1] = ADC12MEM3;                     // Read conversion result
    	ADCvar[1] += ADC12MEM3;                     // Read conversion result
    	break;
    case ADC12IV_ADC12IFG4:   		        // Vector 20:  ADC12MEM4
		//ADCvar[2] = ADC12MEM4;                     // Read conversion result
    	ADCvar[2] += ADC12MEM4;                     // Read conversion result
		break;
    case ADC12IV_ADC12IFG5:   		        // Vector 22:  ADC12MEM5
		//ADCvar[3] = ADC12MEM5;                     // Read conversion result
		ADCvar[3] += ADC12MEM5;                     // Read conversion result
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 4
		isConversionDone = 1;
#endif
		break;
    case ADC12IV_ADC12IFG6:                 // Vector 24:  ADC12MEM6
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
		//ADCvar[4] = ADC12MEM6;                     // Read conversion result
		ADCvar[4] += ADC12MEM6;                     // Read conversion result
		isConversionDone = 1;
		iSamplesRead++;
#endif
		break;
    case ADC12IV_ADC12IFG7:   break;        // Vector 26:  ADC12MEM7
    case ADC12IV_ADC12IFG8:   break;        // Vector 28:  ADC12MEM8
    case ADC12IV_ADC12IFG9:   break;        // Vector 30:  ADC12MEM9
    case ADC12IV_ADC12IFG10:  break;        // Vector 32:  ADC12MEM10
    case ADC12IV_ADC12IFG11:  break;        // Vector 34:  ADC12MEM11
    case ADC12IV_ADC12IFG12:  break;        // Vector 36:  ADC12MEM12
    case ADC12IV_ADC12IFG13:  break;        // Vector 38:  ADC12MEM13
    case ADC12IV_ADC12IFG14:  break;        // Vector 40:  ADC12MEM14
    case ADC12IV_ADC12IFG15:  break;        // Vector 42:  ADC12MEM15
    case ADC12IV_ADC12IFG16:  break;        // Vector 44:  ADC12MEM16
    case ADC12IV_ADC12IFG17:  break;        // Vector 46:  ADC12MEM17
    case ADC12IV_ADC12IFG18:  break;        // Vector 48:  ADC12MEM18
    case ADC12IV_ADC12IFG19:  break;        // Vector 50:  ADC12MEM19
    case ADC12IV_ADC12IFG20:  break;        // Vector 52:  ADC12MEM20
    case ADC12IV_ADC12IFG21:  break;        // Vector 54:  ADC12MEM21
    case ADC12IV_ADC12IFG22:  break;        // Vector 56:  ADC12MEM22
    case ADC12IV_ADC12IFG23:  break;        // Vector 58:  ADC12MEM23
    case ADC12IV_ADC12IFG24:  break;        // Vector 60:  ADC12MEM24
    case ADC12IV_ADC12IFG25:  break;        // Vector 62:  ADC12MEM25
    case ADC12IV_ADC12IFG26:  break;        // Vector 64:  ADC12MEM26
    case ADC12IV_ADC12IFG27:  break;        // Vector 66:  ADC12MEM27
    case ADC12IV_ADC12IFG28:  break;        // Vector 68:  ADC12MEM28
    case ADC12IV_ADC12IFG29:  break;        // Vector 70:  ADC12MEM29
    case ADC12IV_ADC12IFG30:  break;        // Vector 72:  ADC12MEM30
    case ADC12IV_ADC12IFG31:  break;        // Vector 74:  ADC12MEM31
    case ADC12IV_ADC12RDYIFG: break;        // Vector 76:  ADC12RDY
    default: break;
  }
}

// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) Port_2 (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(P2IV, P2IV_P2IFG7)
)
{
	case P2IV_NONE: break;
	case P2IV_P2IFG0: break;
	case P2IV_P2IFG1:
	break;
	case P2IV_P2IFG2:
	//P3OUT &= ~BIT4;                           // buzzer off
	iStatus &= ~BUZZER_ON;
	break;

	case P2IV_P2IFG3: break;
	case P2IV_P2IFG4: break;
	case P2IV_P2IFG5: break;
	case P2IV_P2IFG6: break;
	case P2IV_P2IFG7: break;
	default: break;
}
}

// Port 4 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) Port_4 (void)
#else
#error Compiler not supported!
#endif
{
switch(__even_in_range(P4IV, P4IV_P4IFG7)
)
{
	case P4IV_NONE: break;
	case P4IV_P4IFG0: break;
	case P4IV_P4IFG1:
	iDisplayId = (iDisplayId + 1) % MAX_DISPLAY_ID;
	break;
	case P4IV_P4IFG2: break;
	case P4IV_P4IFG3: break;
	case P4IV_P4IFG4: break;
	case P4IV_P4IFG5: break;
	case P4IV_P4IFG6: break;
	case P4IV_P4IFG7: break;
	default: break;
}
}
