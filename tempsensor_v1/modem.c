/*
 * modem.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include <msp430.h>

#include "config.h"
#include "modem.h"
#include "common.h"
#include "driverlib.h"
#include "stdlib.h"
#include "string.h"
#include "uart.h"
#include "MMC.h"
#include "pmm.h"
#include "lcd.h"
#include "globals.h"
#include "sms.h"
#include "timer.h"

char ctrlZ[2] = { 0x1A, 0 };
char ESC[2] = { 0x1B, 0 };

void modem_checkSignal() {
	if (uart_tx("AT+CSQ\r\n") != UART_SUCCESS)
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

	uart_resetbuffer();
	lcd_disable_debug();
	uart_tx("AT+CGSN\r\n");
	if (uart_rx_cleanBuf(ATCMD_CGSN, ATresponse,
			sizeof(ATresponse))==UART_SUCCESS) {

		if (memcmp(ATresponse, "INVALID", strlen("INVALID"))) {
			//copy valid IMEI to FRAM
			IMEI_OK = true;
		} else {
			lcd_clear();
			lcd_print_line("IMEI Error", LINE2);
		}
	}

	if (!IMEI_OK)
		return;

	if ((uint8_t) g_pInfoA->cfgIMEI[0] == 0xFF)  // IMEI Was not setup, copy it to permament memory
		strcpy(g_pInfoA->cfgIMEI, ATresponse);

	// Lets check if we have the right IMEI from the modem, otherwise we flash it again into config.
	if (memcmp(ATresponse, g_pInfoA->cfgIMEI, 15)!=0) {
		strcpy(g_pInfoA->cfgIMEI, ATresponse);
	}

}

void modem_getsimcardsinfo() {

	modem_getSMSCenter();
	modem_getIMEI();

	delay(100);
}

void modem_init() {

	uint8_t slot=g_pInfoA->cfgSIMSlot;

	if (slot>1) {
		// Memory not initialized
		g_pInfoA->cfgSIMSlot=slot=0;
	}

	uart_setOKMode();
	uart_tx_nowait(ESC); // Cancel any previous command in case we were reseted

#ifdef ENABLE_SIM_SLOT
	uart_tx("AT#GPIO=2,0,1\r\n");
	if (slot != 2) {
		//enable SIM A (slot 1)
		uart_tx("AT#GPIO=2,0,1\r\n");
		uart_tx("AT#GPIO=4,1,1\r\n");
		uart_tx("AT#GPIO=3,0,1\r\n");
	} else {
		//enable SIM B (slot 2)
		uart_tx("AT#GPIO=2,1,1\r\n");
		uart_tx("AT#GPIO=4,0,1\r\n");
		uart_tx("AT#GPIO=3,1,1\r\n");
	}
#endif
	uart_tx_timeout("AT#SIMDET=0\r\n", MODEM_TX_DELAY2, 10);
	uart_tx_timeout("AT#SIMDET=1\r\n", MODEM_TX_DELAY2, 10);
	//uart_tx("AT#SIMDET=2\r\n");

	uart_tx("AT+CMGF=1\r\n");		   // set sms format to text mode
	uart_tx("AT+CMEE=2\r\n");
	uart_tx("AT#CMEEMODE=1\r\n");
	uart_tx("AT#AUTOBND=2\r\n");
	uart_tx("AT#NITZ=1\r\n");
	uart_tx("AT+CTZU=1\r\n");
	uart_tx("AT&K4\r\n");
	uart_tx("AT&P0\r\n");
	uart_tx("AT&W0\r\n");
	uart_tx("AT+CSDH=1\r\n");
	delay(1000);		//some time to enable SMS,POST to work
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