/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#define SMS_C_

#include "thermalcanyon.h"
#include "stdint.h"
#include "stdio.h"
#include "stringutils.h"
#include "sms.h"
#include "modem_uart.h"
#include "timer.h"
#include "config.h"
#include "common.h"
#include "lcd.h"
#include "string.h"
#include "globals.h"
#include "modem.h"
#include "stdlib.h"
#include "battery.h"
#include "pmm.h"
#include "fatdata.h"
#include "events.h"

// Send back data after an SMS request
void sms_send_data_request(char *number) {
	uint32_t iOffset;
	char data[128];

	//get temperature values
	memset(data, 0, MSG_RESPONSE_LEN);
	rtc_update_time();
	strcat(data, get_simplified_date_string(&g_tmCurrTime));
	for (iOffset = 0; iOffset < MAX_NUM_SENSORS; iOffset++) {
		strcat(data, SensorName[iOffset]);
		strcat(data, "=");
		strcat(data, Temperature[iOffset]);
		strcat(data, "C, ");
	}

	// added for show msg//
	strcat(data, "Battery:");
	strcat(data, itoa_pad(batt_getlevel()));
	strcat(data, "%, ");
	if (P4IN & BIT4)	//power not plugged
	{
		strcat(data, "POWER OUT");
	} else if (((P4IN & BIT6)) && (batt_getlevel() == 100)) {
		strcat(data, "FULL CHARGE");
	} else {
		strcat(data, "CHARGING");
	}
	iOffset = strlen(data);

	sms_send_message_number(number, data);
}

// Gets the message
const char COMMAND_RESULT_CMGR[] = "+CMGR: ";

// +CMGR: <stat>,<length><CR><LF><pdu>
// <stat> - status of the message
// 0 - new message
// 1 - read message
// 2 - stored message not yet sent
// 3 - stored message already sent

extern const char AT_MSG_OK[];

int8_t sms_process_memory_message(int8_t index) {
	int t=0, len=0;
	char *token;
	char msg[SMS_MAX_SIZE];
	char state[16];
	char ok[8];
	char phoneNumber[32];
	char *phone;

	uart_txf("AT+CMGR=%d\r\n", index);

	int8_t uart_state = uart_getTransactionState();
	if (uart_state != UART_SUCCESS)
		return uart_state;

	PARSE_FINDSTR_RET(token, COMMAND_RESULT_CMGR, UART_FAILED);
	PARSE_FIRSTSTRING(token, state, sizeof(state), ",\n", UART_FAILED);
	PARSE_NEXTSTRING(token, phoneNumber, sizeof(phoneNumber), ",\n", UART_FAILED);

	len = strlen(phoneNumber);
	for (t=0; t<len; t++)
		if (phoneNumber[t]=='\"')
			phoneNumber[t]=0;

	PARSE_SKIP(token, "\n", UART_FAILED);

	PARSE_NEXTSTRING(token, msg, sizeof(msg), "\n", UART_FAILED);

	// Jump first \n to get the OK
	PARSE_SKIP(token, "\n", UART_FAILED);
	ok[0]=0;
	PARSE_NEXTSTRING(token, ok, sizeof(ok), "\n", UART_FAILED);
	if (ok[0]!='O' || ok[1]!='K' )
		return UART_FAILED;

	phone = &phoneNumber[1];
	switch (msg[0]) {
	case '1':
		sms_send_data_request(phone); // Send the phone without the \"
		break;
	case '2':
		sms_send_message_number(phone, "Rebooting...");
		delallmsg();
		for (t = 0; t < 10; t++) {
			lcd_printf(LINEC, "REBOOT DEVICE");
			lcd_printf(LINE2, "   *** %d ***  ", (10 - t));
			lcd_progress_wait(1000);
		}
		//reset the board by issuing a SW BOR
		PMM_trigBOR();
		while (1);	//code should not come here
	case 'E':
	case 'e':
		events_send_data(phone);
		break;
	default:
		config_process_configuration(token);
		break;
	}

	return UART_SUCCESS;
}

// Gets how many messages and the max number of messages on the card / memory
// +CPMS: <memr>,<usedr>,<totalr>,<memw>,<usedw>,<totalw>,
// <mems>,<useds>,<totals>

const char COMMAND_RESULT_CPMS[] = "+CPMS: ";

int8_t sms_process_messages() {
	char *token;
	char SM_ME[5]; // Internal memory or sim card used
	uint32_t iIdx;
	SIM_CARD_CONFIG *sim = config_getSIM();
	uint8_t usedr = 0; // Reading memory
	uint8_t totalr = 0;

#ifdef _DEBUG
	config_setLastCommand(COMMAND_SMS_PROCESS);
#endif

	memset(SM_ME, 0, sizeof(SM_ME));

	lcd_printf(LINEC, "Fetching SMS");
	lcd_printf(LINE2, "SIM %d ", config_getSelectedSIM());

	//check if messages are available
	uart_tx("AT+CPMS?\r\n");

	// Failed to get parameters
	int8_t uart_state = uart_getTransactionState();
	if (uart_state != UART_SUCCESS)
		return uart_state;

	PARSE_FINDSTR_RET(token, COMMAND_RESULT_CPMS, UART_FAILED);
	PARSE_FIRSTSTRING(token, SM_ME, sizeof(SM_ME) - 1, ", \n", UART_FAILED);
	PARSE_NEXTVALUE(token, &usedr, ", \n", UART_FAILED);
	PARSE_NEXTVALUE(token, &totalr, ", \n", UART_FAILED);

	if (check_address_empty(sim->iMaxMessages) || sim->iMaxMessages != totalr) {
		sim->iMaxMessages = totalr;
	}

	if (usedr==0) {
		lcd_printf(LINEC, "No new messages", usedr);
		return UART_SUCCESS;
	}

	lcd_printf(LINEC, "%d Config sms", usedr);
	lcd_printl(LINE2, "Msg Processing..");

	uart_tx("AT+CSDH=0\r\n"); // Disable extended output

	for (iIdx = 1; iIdx<=totalr; iIdx++) {
		if (sms_process_memory_message(iIdx)==UART_SUCCESS)
			usedr--;
		else
			_NOP();
	}

	delallmsg();
	uart_tx("AT+CSDH=1\r\n"); // Restore extended output

	return UART_SUCCESS;
}

void sms_send_heart_beat() {
	char msg[SMS_MAX_SIZE];

	char* pcTmp = NULL;
	SIM_CARD_CONFIG *sim = config_getSIM();

	int i = 0;

	lcd_print("SMS SYNC");
	//send heart beat
	memset(msg, 0, sizeof(msg));
	strcat(msg, SMS_HB_MSG_TYPE);
	strcat(msg, g_pDevCfg->cfgIMEI);
	strcat(msg, ",");
	if (config_getSelectedSIM()) {
		strcat(msg, "1,");
	} else {
		strcat(msg, "0,");
	}
	strcat(msg, g_pDevCfg->cfgGatewaySMS);
	strcat(msg, ",");
	strcat(msg, sim->cfgSMSCenter);
	strcat(msg, ",");
	for (i = 0; i < MAX_NUM_SENSORS; i++) {
		if (Temperature[i][0] == '-') {
			strcat(msg, "0,");
		} else {
			strcat(msg, "1,");
		}
	}

	pcTmp = itoa_pad(batt_getlevel());	//opt by directly using tmpstr
	strcat(msg, pcTmp);
	if (P4IN & BIT4) {
		strcat(msg, ",0");
	} else {
		strcat(msg, ",1");
	}

	// Attach Fimware release date
	strcat(msg, ",");
	strcat(msg, g_pSysCfg->firmwareVersion);
#ifdef _DEBUG
	strcat(msg, " dev");
#endif
	strcat(msg, "");

	sms_send_message(msg);
}

extern char* itoa_nopadding(int num);

//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()

uint8_t sms_send_message_number(char *szPhoneNumber, char* pData) {
	char szCmd[64];
	uint16_t msgNumber = 0; // Validation number from the network returned by the CMGS command
	int res = UART_ERROR;
	int verbose = g_iLCDVerbose;
	int phonecode = 129;

	if (g_iStatus & TEST_FLAG)
		return UART_SUCCESS;

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_clear();
		lcd_printf(LINE1, "SYNC SMS %d ", g_pDevCfg->cfgSIM_slot + 1);
		lcd_printl(LINE2, szPhoneNumber);
		lcd_disable_verbose();
	}

	strcat(pData, ctrlZ);

	// 129 - number in national format
	// 145 - number in international format (contains the "+")

	if (szPhoneNumber[0]=='+')
		phonecode = 145;

	sprintf(szCmd, "AT+CMGS=\"%s\",%d\r\n", szPhoneNumber, phonecode);

	uart_setSMSPromptMode();
	if (uart_tx_waitForPrompt(szCmd, TIMEOUT_CMGS_PROMPT)) {
		uart_tx_timeout(pData, TIMEOUT_CMGS, 1);

		// TODO Check if ok or RXBuffer contains Error
		res = uart_rx(ATCMD_CMGS, ATresponse);
		msgNumber = atoi(ATresponse);
	}

	if (verbose == VERBOSE_BOOTING)
		lcd_enable_verbose();

	if (res == UART_SUCCESS) {
		lcd_clear();
		lcd_printl(LINE1, "SMS Confirm    ");
		lcd_printf(LINE2, "MSG %d ", msgNumber);
		delay(HUMAN_DISPLAY_INFO_DELAY);
		_NOP();
	} else if (res == UART_ERROR) {
		lcd_printf(LINE2, "MODEM ERROR     ");
		log_appendf("ERROR: SIM %d FAILED [%s]", config_getSelectedSIM(),
				config_getSIM()->simLastError);
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	} else {
		lcd_print("MODEM TIMEOUT");
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	}

	_NOP();
	return res;
}

uint8_t sms_send_message(char* pData) {
	return sms_send_message_number(NEXLEAF_SMS_GATEWAY, pData);
}

int sms_recv_message(int8_t iMsgIdx, char* pData) {
	int ret = -1;

	//uart_tx("AT+CMGL=\"REC UNREAD\"\r\n");
	//uart_tx("AT+CMGL=\"REC READ\"\r\n");
	//uart_tx("AT+CMGL=\"ALL\"\r\n");

	uart_txf("AT+CMGR=%d\r\n", iMsgIdx);
	ret = uart_rx(ATCMD_CMGR, pData);	//copy RX to pData

	return ret;
}

void delallmsg() {
	uart_tx("AT+CMGD=1,2\r\n");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx, char* pData) {
	char szCmd[64];
	sprintf(szCmd, "AT+CMGD=%d,0\r\n", iMsgIdx);
	uart_tx(szCmd);
	delay(2000);	//opt
}

