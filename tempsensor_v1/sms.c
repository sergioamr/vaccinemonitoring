/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#if 1
#define SMS_C_

#include "string.h"
#include "stdint.h"
#include "sms.h"
#include "uart.h"
#include "timer.h"
#include "config.h"
#include "common.h"
extern char* itoa_nopadding(int num);
extern int8_t iStatus;



//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()



void sendmsg(char* pData)
{
	  if(iStatus & TEST_FLAG) return;
	  //char ctrlZ[2] = {0x1A,0};
	  //uart_tx("AT+CMGS=\"8151938952\",129\r\n");

	  //uart_tx("AT+CMGS=\"+918151938952\",145\r\n");
	  uart_tx("AT+CMGS=\"00447751035864\",129\r\n");
	  //uart_tx("AT+CMGS=\"9900029739\",129\r\n");
	  //uart_tx("AT+CMGS=\"9900029636\",129\r\n"); //ZZZZ take an array of DA and send SMS to each one
	  //uart_tx("AT+CMGS=\"919900029636\",129\r\n"); //ZZZZ take an array of DA and send SMS to each one
	  delay(5000);
	  uart_tx(pData);
	  delay(1000);
	  //uart_tx(ctrlZ);
}

int recvmsg(int8_t iMsgIdx, char* pData)
{
	int ret = -1;

	//reset the RX counters to reuse memory from POST DATA
	RXTailIdx = RXHeadIdx = 0;
    iRxLen = RX_EXTENDED_LEN;
	RX[RX_EXTENDED_LEN+1] = 0;	//null termination

	//uart_tx("AT+CMGL=\"REC UNREAD\"\r\n");
	//uart_tx("AT+CMGL=\"REC READ\"\r\n");
	//uart_tx("AT+CMGL=\"ALL\"\r\n");

	strcat(pData, "AT+CMGR="); //resuse to format CMGR (sms read)
	strcat(pData,itoa_nopadding(iMsgIdx));
	strcat(pData, "\r\n");
	uart_tx(pData);
	delay(1000);	//opt
	ret = uart_rx(ATCMD_CMGR,pData);	//copy RX to pData

	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;

	return ret;
}

void delallmsg()
{
	uart_tx("AT+CMGD=1,2\r\n");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx, char* pData)
{

	strcat(pData, "AT+CMGD=");
	strcat(pData,itoa_nopadding(iMsgIdx));
	strcat(pData,",0\r\n");
	uart_tx(pData);
	delay(2000);	//opt
}
#endif
