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
		delay(MODEM_TX_DELAY1);     // TODO Check why it was 5000
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
		delay(MODEM_TX_DELAY1);     // TODO Check why it was 5000
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
