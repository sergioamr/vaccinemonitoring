/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#if 1
#define SMS_C_

#include "stdint.h"
#include "sms.h"
#include "uart.h"
#include "timer.h"
#include "config.h"
#include "common.h"
#include "lcd.h"
#include "string.h"
#include "globals.h"
#include "modem.h"

extern char* itoa_nopadding(int num);

//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()

char destinationSMS[] = "AT+CMGS=\"" SMS_NEXLEAF_GATEWAY "\",129\r\n";
void sendmsg(char* pData) {
	if (iStatus & TEST_FLAG)
		return;

	if (g_iDebug_state==0) {
		lcd_clear();
		lcd_print_line("SMS To ", LINE1);
		lcd_print_line(SMS_NEXLEAF_GATEWAY, LINE2);
	}

	lcd_disable_debug();
	strcat(pData,ctrlZ);

	if (uart_tx_waitForPrompt(destinationSMS)) {
		uart_tx(pData);
		delay(5000);
		_NOP();
		// TODO Check if ok or RXBuffer contains Error
	}

	int res=uart_rx(ATCMGS, ATresponse);

	if (res==UART_SUCCESS) {
		_NOP();
	} else
	if (res==UART_ERROR) {
		lcd_print("MODEM ERROR");
	}

	_NOP();
}

int recvmsg(int8_t iMsgIdx, char* pData) {
	int ret = -1;

	//reset the RX counters to reuse memory from POST DATA
	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_EXTENDED_LEN;
	RXBuffer[RX_EXTENDED_LEN + 1] = 0; //null termination ... Outside the array??? wtf

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
