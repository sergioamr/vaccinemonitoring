/*
 * modem.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include <msp430.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "driverlib.h"
#include "config.h"
#include "modem.h"
#include "common.h"
#include "uart.h"
#include "MMC.h"
#include "pmm.h"
#include "lcd.h"
#include "globals.h"
#include "sms.h"
#include "timer.h"
#include "time.h"
#include "rtc.h"
#include "http.h"

char ctrlZ[2] = { 0x1A, 0 };
char ESC[2] = { 0x1B, 0 };

void modem_swapSIM() {
	config_incLastCmd();

	if (g_pInfoA->cfgSIMSlot != 1) {
		//current sim slot is 1
		//change to sim slot 2
		g_pInfoA->cfgSIMSlot = 1;
		lcd_print_lne(LINE2, "Switching SIM: 2");
		delay(100);
	} else {
		//current sim slot is 2
		//change to sim slot 1
		g_pInfoA->cfgSIMSlot = 0;
		lcd_print_lne(LINE2, "Switching SIM: 1");
		delay(100);
	}

	modem_init();
}

void modem_checkSignal() {
	config_incLastCmd();

	if (uart_tx_timeout("AT+CSQ\r\n", TIMEOUT_CSQ, 1) != UART_SUCCESS)
		return;

	uart_rx_cleanBuf(ATCMD_CSQ, ATresponse, sizeof(ATresponse));
	if (ATresponse[0] != 0) {
		iSignalLevel = strtol(ATresponse, 0, 10);
		if ((iSignalLevel <= NETWORK_DOWN_SS)
				|| (iSignalLevel >= NETWORK_MAX_SS)) {
			signal_gprs = 0;
		} else {
			signal_gprs = 1;
		}
	}
}

void modem_getSMSCenter() {
	uint8_t slot = g_pInfoA->cfgSIMSlot;
	config_incLastCmd();

	// Reading the Service Center Address to use as message gateway
	// http://www.developershome.com/sms/cscaCommand.asp
	// Get service center address; format "+CSCA: address,address_type"

	// added for SMS Message center number to be sent in the heart beat
	uart_resetbuffer();
	uart_tx("AT+CSCA?\r\n");

	if (uart_rx_cleanBuf(ATCMD_CSCA, ATresponse,
			sizeof(ATresponse))==UART_SUCCESS) {

		memcpy(&g_pInfoA->cfgSMSCenter[slot][0], ATresponse,
				strlen(ATresponse));

	}
}

void modem_getIMEI() {
	// added for IMEI number//
	char IMEI_OK = false;
	config_incLastCmd();

	uart_resetbuffer();
	uart_tx("AT+CGSN\r\n");
	if (uart_rx_cleanBuf(ATCMD_CGSN, ATresponse,
			sizeof(ATresponse))==UART_SUCCESS) {

		if (memcmp(ATresponse, "INVALID", strlen("INVALID"))) {
			//copy valid IMEI to FRAM
			IMEI_OK = true;
		} else {
			lcd_clear();
			lcd_print_lne(LINE2, "IMEI Error");
		}
	}

	if (!IMEI_OK)
		return;

	if ((uint8_t) g_pInfoA->cfgIMEI[0] == 0xFF) // IMEI Was not setup, copy it to permament memory
		strcpy(g_pInfoA->cfgIMEI, ATresponse);

	// Lets check if we have the right IMEI from the modem, otherwise we flash it again into config.
	if (memcmp(ATresponse, g_pInfoA->cfgIMEI, 15) != 0) {
		strcpy(g_pInfoA->cfgIMEI, ATresponse);
	}

}

void modem_getExtraInfo() {

	modem_getIMEI();
	delay(100);
}

// 2 minutes timeout by the TELIT documentation
#define MAX_CSURV_TIME 120000

#ifdef _DEBUG
#define NETWORK_WAITING_TIME 5000
#define NET_ATTEMPTS 10
#else
#define NETWORK_WAITING_TIME 10000
#define NET_ATTEMPTS 10
#endif

void modem_surveyNetwork() {

	int attempts = NET_ATTEMPTS;
	int uart_state;
	int slot = g_pInfoA->cfgSIMSlot;
	config_incLastCmd();

	if (g_pInfoA->iCfgMCC[slot] != 0 && g_pInfoA->iCfgMNC[slot] != 0)
		return;

	lcd_clear();
	lcd_disable_verbose();

	uart_setDelayIntervalDivider(64);  // 120000 / 64

	do {
		if (attempts != NET_ATTEMPTS) {
			lcd_clear();
			sprintf(g_szTemp, "MCC RETRY %d   ", NET_ATTEMPTS - attempts);
			lcd_print_lne(LINE1, g_szTemp);
		} else {
			lcd_print_lne(LINE1, "MCC DISCOVER");
		}

		uart_resetbuffer();

		// Displays info for the only serving cell
		uart_tx("AT#CSURVEXT=0\r\n");
		uart_state = uart_getTransactionState();

		if (uart_state == UART_SUCCESS) {

			// We just want only one full buffer. The data is on the first few characters of the stream
			uart_setNumberOfPages(1);
			uart_tx_timeout("AT#CSURV\r\n", TIMEOUT_CSURV, 10); // #CSURV - Network Survey
			// Maximum timeout is 2 minutes

			//Execution command allows to perform a quick survey through channels
			//belonging to the band selected by last #BND command issue, starting from
			//channel <s> to channel <e>. If parameters are omitted, a full band scan is
			//performed.

			uart_state = uart_getTransactionState();
			if (uart_state != UART_SUCCESS) {
				lcd_print_lne(LINE2, "NETWORK BUSY");
				delay(NETWORK_WAITING_TIME);
			} else if (uart_rx_cleanBuf(ATCMD_CSURVC, ATresponse,
					sizeof(ATresponse)) == UART_SUCCESS) {
				// Get MCC then MNC (the next in the list)
				char* surveyResult = strtok(ATresponse, ",");
				g_pInfoA->iCfgMCC[slot] = atoi(surveyResult);
				surveyResult = strtok(NULL, ",");
				g_pInfoA->iCfgMNC[slot] = atoi(surveyResult);

				lcd_clear();
				if (g_pInfoA->iCfgMCC[slot] > 0
						&& g_pInfoA->iCfgMNC[slot] > 0) {
					lcd_print_lne(LINE1, "SUCCESS");
					lcd_print_ext(LINE2, "MCC %d MNC %d",
							g_pInfoA->iCfgMCC[slot], g_pInfoA->iCfgMNC[slot]);
				} else {
					uart_state = UART_ERROR;
					lcd_print_lne(LINE2, "FAILED");
					delay(1000);
				}
			}
		}

		attempts--;
	} while (uart_state != UART_SUCCESS && attempts > 0);

	uart_setDefaultIntervalDivider();

	lcd_enable_verbose();
	lcd_progress_wait(10000); // Wait 10 seconds to make sure the modem finished transfering.
							  // It should be clear already but next transaction
}

void modem_parse_time(char* pDatetime, struct tm* pTime) {
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

void modem_pull_time() {
	int i;
	for (i = 0; i < MAX_TIME_ATTEMPTS; i++) {
		uart_tx("AT+CCLK?\r\n");
		uart_rx_cleanBuf(ATCMD_CCLK, ATresponse, sizeof(ATresponse));
		modem_parse_time(ATresponse, &currTime);
		rtc_init(&currTime);

		if (currTime.tm_year!=0) {
			// Day has changed so save the new date TODO keep trying until date is set. Call function ONCE PER DAY
			g_iCurrDay = currTime.tm_mday;
			break;
		}
	}
}

void modem_init() {

	config_setLastCommand(COMMAND_MODEMINIT);

	uint8_t slot = g_pInfoA->cfgSIMSlot;

	if (slot > 1) {
		// Memory not initialized
		g_pInfoA->cfgSIMSlot = slot = 0;
	}

	uart_setOKMode();

	lcd_disable_verbose();
	uart_tx_nowait(ESC); // Cancel any previous command in case we were reseted
	uart_tx_timeout("AT\r\n", TIMEOUT_DEFAULT, 10); // Loop for OK until modem is ready
	lcd_enable_verbose();

	uart_tx("AT\r\n"); // Display OK

#ifdef ENABLE_SIM_SLOT
	if (slot != 1) {
		//enable SIM A (slot 1)
		uart_tx_timeout("AT#GPIO=2,0,1\r\n", TIMEOUT_GPO, 5); // First command always has a chance of timeout
		uart_tx("AT#GPIO=4,1,1\r\n");
		uart_tx("AT#GPIO=3,0,1\r\n");
	} else {
		//enable SIM B (slot 2)
		uart_tx_timeout("AT#GPIO=2,1,1\r\n", TIMEOUT_GPO, 5);
		uart_tx("AT#GPIO=4,0,1\r\n");
		uart_tx("AT#GPIO=3,1,1\r\n");
	}
#endif
	uart_tx_timeout("AT#SIMDET=0\r\n", MODEM_TX_DELAY2, 10);
	uart_tx_timeout("AT#SIMDET=1\r\n", MODEM_TX_DELAY2, 10);
	//uart_tx("AT#SIMDET=2\r\n");

	uart_tx_timeout("AT+CMGF=1\r\n", MODEM_TX_DELAY2, 5); // set sms format to text mode
	uart_tx("AT+CMEE=2\r\n");
	uart_tx("AT#CMEEMODE=1\r\n");
	uart_tx("AT#AUTOBND=2\r\n");

	uart_tx("AT#NITZ=1\r\n");
	uart_tx("AT+CTZU=1\r\n");
	uart_tx("AT&K4\r\n");
	uart_tx("AT&P0\r\n");
	uart_tx("AT&W0\r\n");
	uart_tx("AT+CSDH=1\r\n");

	modem_getSMSCenter();
	modem_checkSignal();

#ifndef _DEBUG
	modem_surveyNetwork();
#endif

#ifdef POWER_SAVING_ENABLED
	uart_tx("AT+CFUN=5\r\n"); //full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);

	uart_tx("AT+CFUN?\r\n");//full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);
#endif

	modem_pull_time();

	lcd_print("Checking GPRS");
	/// added for gprs connection..//
	signal_gprs = dopost_gprs_connection_status(GPRS);

	// Disable echo from modem
	uart_tx("ATE0\r\n");
}

#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode() {

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

int8_t modem_exit_powersave_mode() {

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
