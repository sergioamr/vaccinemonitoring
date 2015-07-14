/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#define SMS_C_

#include "thermalcanyon.h"

// Send back data after an SMS request
void sms_send_data_request(char *number) {
	uint32_t iOffset;
	char *data = getSMSBufferHelper();

	if (number == NULL || strlen(number) == 0)
		return;

	//get temperature values
	rtc_update_time();
	strcat(data, get_simplified_date_string(&g_tmCurrTime));
	strcat(data, " ");
	for (iOffset = 0; iOffset < SYSTEM_NUM_SENSORS; iOffset++) {
		strcat(data, SensorName[iOffset]);
		strcat(data, ":");
		strcat(data, temperature_getString(iOffset));
		strcat(data, "C, ");
	}

	// added for show msg//
	strcat(data, "BATTERY:");
	strcat(data, itoa_nopadding(batt_getlevel()));
	strcat(data, "%, ");
	if (P4IN & BIT4)	//power not plugged
	{
		strcat(data, "POWER OUT");
	} else if (((P4IN & BIT6)) && (batt_getlevel() == 100)) {
		strcat(data, "FULL CHARGE");
	} else {
		strcat(data, "CHARGING");
	}

#ifdef _DEBUG
	strcat(data, ",UPTIME:");
	strcat(data, itoa_nopadding(iMinuteTick));
#endif

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
	int t = 0, len = 0;
	char *token;
	char *msg = getSMSBufferHelper();
	char state[16];
	char ok[8];
	char phoneNumber[32];
	char *phone;
	char answer = 0;

	uart_txf("+CMGR=%d", index);

	int8_t uart_state = uart_getTransactionState();
	if (uart_state != UART_SUCCESS)
		return uart_state;

	PARSE_FINDSTR_RET(token, COMMAND_RESULT_CMGR, UART_FAILED);
	PARSE_FIRSTSTRING(token, state, sizeof(state), ",\n", UART_FAILED);
	PARSE_NEXTSTRING(token, phoneNumber, sizeof(phoneNumber), ",\n",
			UART_FAILED);

	len = strlen(phoneNumber);
	for (t = 0; t < len; t++)
		if (phoneNumber[t] == '\"')
			phoneNumber[t] = 0;

	PARSE_SKIP(token, "\n", UART_FAILED);

	PARSE_NEXTSTRING(token, msg, MAX_SMS_SIZE, "\n", UART_FAILED);

	// Jump first \n to get the OK
	PARSE_SKIP(token, "\n", UART_FAILED);
	ok[0] = 0;
	PARSE_NEXTSTRING(token, ok, sizeof(ok), "\n", UART_FAILED);
	if (ok[0] != 'O' || ok[1] != 'K')
		return UART_FAILED;

	phone = &phoneNumber[1];
	switch (msg[0]) {
	case '1':
		sms_send_data_request(phone); // Send the phone without the \"
		break;
	case 'R':
	case '2':
		sms_send_message_number(phone, "Rebooting...");
		delallmsg();
		for (t = 0; t < 10; t++) {
			lcd_printf(LINEC, "REBOOT DEVICE");
			lcd_printf(LINE2, "   *** %d ***  ", (10 - t));
			lcd_progress_wait(1000);
		}

		//reset the board by issuing a SW BOR
		system_reboot("NET_COMMAND");
	case 'E':
		events_send_data(phone);
		break;
	case 'C':
		config_send_configuration(phone);
		break;
	case 'U':
		event_force_event_by_id(EVT_SUBSAMPLE_TEMP, 5);
		event_force_event_by_id(EVT_SAVE_SAMPLE_TEMP, 10);
		event_force_event_by_id(EVT_UPLOAD_SAMPLES, 15);
		answer = 1;
		break;
	case 'A':
		state_alarm_on("SMS_TRIGGER");
		answer = 1;
		break;

	default:
		if (msg[0] != '$')
			return UART_SUCCESS;

		if (config_parse_configuration(msg) == UART_SUCCESS)
			answer = 1;
		else {
			sms_send_message_number(phone, "FAILED");
			return UART_SUCCESS;
		}
		break;
	}

	if (answer)
		sms_send_message_number(phone, "ACK");

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

	config_setLastCommand(COMMAND_SMS_PROCESS);

	memset(SM_ME, 0, sizeof(SM_ME));

	lcd_printf(LINEC, "Fetching SMSs");
	lcd_printf(LINE2, "SIM %d", config_getSelectedSIM() + 1);

	//check if messages are available
	uart_tx("+CPMS?");

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

	if (usedr == 0) {
		lcd_printf(LINEC, "No new messages", usedr);
		return UART_SUCCESS;
	}

	lcd_printf(LINEC, "%d SMS Fetch", usedr);
	lcd_printl(LINE2, "Msg Processing..");

	uart_tx("+CSDH=0"); // Disable extended output

	for (iIdx = 1; iIdx <= totalr; iIdx++) {
		if (sms_process_memory_message(iIdx) == UART_SUCCESS) {
			usedr--;
			delmsg(iIdx);
		} else
			_NOP();
	}

	delallmsg();
	uart_tx("+CSDH=1"); // Restore extended output

	return UART_SUCCESS;
}

void sms_send_heart_beat() {
	char *msg = getSMSBufferHelper();
	char sensors[16];

	SIM_CARD_CONFIG *sim = config_getSIM();
	SYSTEM_ALARMS *s = state_getAlarms(); //pointer to alarm states

	int i = 0;

	//send heart beat
	sensors[0] = 0;
	for (i = 0; i < SYSTEM_NUM_SENSORS; i++) {
		if (temperature_getString(i)[0] == '-') {
			strcat(sensors, "0,");
		} else {
			strcat(sensors, "1,");
		}
	}
	//current: SENSORS, BATTERY, CHARGING_STATUS, UPTIME (what time unit?)
	//new: SENSORS, SD_CARD_ERROR, UPLOAD_FAILURES, BATTERY, CHARGING_STATUS, UPTIME
	sprintf(msg, SMS_HB_MSG_TYPE "%s,%d,%s,%s,%s%d,%d", g_pDevCfg->cfgIMEI,
			config_getSelectedSIM(), g_pDevCfg->cfgGatewaySMS,
			sim->cfgSMSCenter, sensors, batt_getlevel(), !(P4IN & BIT4));

	sms_send_message(msg);
}

uint8_t sms_send_message_number(char *szPhoneNumber, char* pData) {
	char szCmd[64];
	uint16_t msgNumber = 0; // Validation number from the network returned by the CMGS command
	int res = UART_ERROR;
	int verbose = g_iLCDVerbose;
	int phonecode = 129;
	char *token;
	SIM_CARD_CONFIG *sim = config_getSIM();

	state_SMS_lastMessageACK(sim, -1);

	if (szPhoneNumber == NULL || strlen(szPhoneNumber) == 0)
		return UART_SUCCESS;

	if (pData == NULL || strlen(pData) == 0)
		return UART_SUCCESS;

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_clear();
		lcd_printf(LINE1, "SYNC SMS %d", g_pDevCfg->cfgSIM_slot + 1);
		lcd_printl(LINE2, szPhoneNumber);
		lcd_disable_verbose();
	}

	strcat(pData, ctrlZ);

	// 129 - number in national format
	// 145 - number in international format (contains the "+")

	if (szPhoneNumber[0] == '+')
		phonecode = 145;

	http_deactivate();
	sprintf(szCmd, "AT+CMGS=\"%s\",%d\r\n", szPhoneNumber, phonecode);

	uart_setSMSPromptMode();
	if (uart_tx_waitForPrompt(szCmd, TIMEOUT_CMGS_PROMPT)) {
		uart_tx_data(pData, TIMEOUT_CMGS, 1);

		token = strstr(uart_getRXHead(), "ERROR");
		if (token == NULL) {
			token = strstr(uart_getRXHead(), "+CMGS:");
			if (token != NULL) {
				msgNumber = atoi(token + 6);
				state_SMS_lastMessageACK(sim, msgNumber);
			}
			if (msgNumber != 0)
				res = UART_SUCCESS;
			else
				res = UART_ERROR;
		} else {
			res = UART_ERROR;
		}
	}

	if (verbose == VERBOSE_BOOTING)
		lcd_enable_verbose();

	if (res == UART_SUCCESS) {
		lcd_clear();
		lcd_printl(LINE1, "SMS Confirm");
		lcd_printf(LINE2, "MSG %d", msgNumber);
		delay(HUMAN_DISPLAY_INFO_DELAY);
		_NOP();
	} else if (res == UART_ERROR) {

	} else {
		lcd_print("TIMEOUT");
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	}

	_NOP();
	return res;
}

uint8_t sms_send_message(char* pData) {
	return sms_send_message_number(g_pDevCfg->cfgGatewaySMS, pData);
}

void delallmsg() {
	uart_tx("+CMGD=1,2");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx) {
	uart_txf("+CMGD=%d,0", iMsgIdx);
	lcd_progress_wait(1000);
}

