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
int doget() {
	uart_resetbuffer();
	iRxLen = RX_LEN;
	RXBuffer[RX_LEN] = 0;	//null termination

	strcpy(g_szTemp, "AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/");
	strcat(g_szTemp, g_pInfoA->cfgIMEI);
	strcat(g_szTemp, "/1/\"\r\n");
	uart_tx_timeout(g_szTemp, 10000, 1);

	uart_tx_timeout("AT#HTTPRCV=1\r\n",180000,1);		//get the configuartion

	memset(ATresponse, 0, sizeof(ATresponse));
	uart_rx(ATCMD_HTTPRCV, ATresponse);

	uart_resetbuffer();
	iRxLen = RX_LEN;
	return 0; // TODO return was missing, is it necessary ?
}


