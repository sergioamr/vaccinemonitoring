/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#if 1
#define SMS_C_

#include "stdint.h"
#include "stdio.h"
#include "stringutils.h"
#include "sms.h"
#include "uart.h"
#include "timer.h"
#include "config.h"
#include "common.h"
#include "lcd.h"
#include "string.h"
#include "globals.h"
#include "modem.h"
#include "stdlib.h"

extern char* itoa_nopadding(int num);

//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()

uint8_t sendmsg_number(char *szPhoneNumber, char* pData) {

	uint16_t msgNumber=0;  // Validation number from the network returned by the CMGS command
	int res=UART_ERROR;

	if (iStatus & TEST_FLAG)
		return UART_SUCCESS;

	if (g_iLCDVerbose == 0) {
		lcd_clear();
		lcd_print_line("Sync SMS To ", LINE1);
		lcd_print_line(szPhoneNumber, LINE2);
	}

	lcd_disable_verbose();
	strcat(pData, ctrlZ);

	sprintf(g_szTemp, "AT+CMGS=\"%s\",129\r\n", szPhoneNumber);
	if (uart_tx_waitForPrompt(g_szTemp)) {
		uart_tx_timeout(pData, TIMEOUT_CMGS, 1);

		// TODO Check if ok or RXBuffer contains Error
		res = uart_rx(ATCMD_CMGS, ATresponse);
		msgNumber = atoi(ATresponse);
	}

	if (res == UART_SUCCESS) {
		sprintf(g_szTemp, "MSG %d ", msgNumber);
		lcd_print_line(g_szTemp,LINE2);
		_NOP();
	} else if (res == UART_ERROR) {
		lcd_print("MODEM ERROR");
	}

	_NOP();
	return res;
}

uint8_t sendmsg(char* pData) {
	return sendmsg_number(SMS_NEXLEAF_GATEWAY, pData);
}

int recvmsg(int8_t iMsgIdx, char* pData) {
	int ret = -1;

	//reset the RX counters to reuse memory from POST DATA
	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;
	RXBuffer[RX_LEN] = 0; //null termination ... Outside the array??? wtf

	//uart_tx("AT+CMGL=\"REC UNREAD\"\r\n");
	//uart_tx("AT+CMGL=\"REC READ\"\r\n");
	//uart_tx("AT+CMGL=\"ALL\"\r\n");

	strcat(pData, "AT+CMGR="); //resuse to format CMGR (sms read)
	strcat(pData, itoa_nopadding(iMsgIdx));
	strcat(pData, "\r\n");
	uart_tx(pData);
	delay(1000);	//opt
	ret = uart_rx(ATCMD_CMGR, pData);	//copy RX to pData

	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;

	return ret;
}

void delallmsg() {
	uart_tx("AT+CMGD=1,2\r\n");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx, char* pData) {

	strcat(pData, "AT+CMGD=");
	strcat(pData, itoa_nopadding(iMsgIdx));
	strcat(pData, ",0\r\n");
	uart_tx(pData);
	delay(2000);	//opt
}
#endif
