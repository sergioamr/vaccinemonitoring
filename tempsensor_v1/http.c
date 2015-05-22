/*
 * http.c
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n");

#include "thermalcanyon.h"

void dohttpsetup() {

	// TODO Set correct APN
	//char* apn = &g_pInfoA->cfgAPN[g_pInfoA->cfgSIMSlot][0];

	uart_tx("AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n"); //APN
	uart_tx("AT#SGACT=1,1\r\n");

	// LONG TIMEOUT
	uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
}

void deactivatehttp() {

	// LONG TIMEOUT
	uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
}

//WARNING: doget() should not be used in parallel to HTTP POST
int doget(char* queryData) {
	uart_resetbuffer();
	iRxLen = RX_EXTENDED_LEN;
	RXBuffer[RX_EXTENDED_LEN + 1] = 0;	//null termination
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

	uart_resetbuffer();
	iRxLen = RX_LEN;
	return 0; // TODO return was missing, is it necessary ?
}


