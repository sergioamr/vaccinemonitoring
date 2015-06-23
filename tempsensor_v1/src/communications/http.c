/*
 * http.c
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n");
#include "thermalcanyon.h"
#include "stringutils.h"
#include "config.h"

#define HTTP_RESPONSE_RETRY	10

void backend_get_configuration() {
	http_enable();
	http_get_configuration();
	http_deactivate();
}

void full_backend_get_configuration() {

	config_setLastCommand(COMMAND_HTTP_DATA_TRANSFER);
	lcd_print("NEXLEAF PING");
	if (modem_check_network() != UART_SUCCESS) {
		return;
	}

	if (http_setup() == UART_SUCCESS) {
		http_get_configuration();
		http_deactivate();
	}
}

uint8_t http_enable() {
	int attempts = HTTP_COMMAND_ATTEMPTS;
	int uart_state = UART_FAILED;
	SIM_CARD_CONFIG *sim = config_getSIM();

	// Context Activation - #SGACT
	// Execution command is used to activate or deactivate either the GSM context
	// or the specified PDP context.
	//  1..5 - numeric parameter which specifies a particular PDP context definition
	//  1 - activate the context

	do {
		uart_tx("AT#SGACT=1,1\r\n");
		// CME ERROR: 555 Activation failed
		// CME ERROR: 133 Requested service option not subscribed
		uart_state = uart_getTransactionState();
		if (uart_state != UART_SUCCESS) {
			delay(1000);
		} else {
			config_reset_error(sim);
		}

	} while (uart_state != UART_SUCCESS && --attempts > 0);

	return uart_state;
}

int8_t http_setup() {
	int uart_state = UART_FAILED;
	SIM_CARD_CONFIG *sim = config_getSIM();

	if (sim->cfgAPN[0] == '\0')
		return UART_FAILED;

	lcd_printf(LINEC, "HTTP SIM %d", config_getSelectedSIM() + 1);

	uart_txf("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0\r\n", sim->cfgAPN); //APN
	if (uart_getTransactionState() != UART_SUCCESS)
		return UART_FAILED;

	lcd_progress_wait(1000);

	uart_state = http_enable();
	// We were not successful trying to activate the HTTP transaction
	if (uart_state != UART_SUCCESS) {
		return UART_FAILED;
	}

	// LONG TIMEOUT
	// Prof id, server addresws, server port, auth type, username, password, ssl_enabled, timeout, cid
	uart_txf("AT#HTTPCFG=1,\"%s\",80,0\r\n", g_pDevCfg->cfgGatewayIP);
	if (uart_getTransactionState() != UART_SUCCESS) {
		lcd_printl(LINE2, "FAILED");
		return UART_FAILED;
	}

	lcd_printl(LINE2, "SUCCESS");
	return UART_SUCCESS;
}

uint8_t http_deactivate() {
	// LONG TIMEOUT
	return uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
}

const char HTTP_INCOMING_DATA[] = { 0x0D, 0x0A, '<', '<', '<', 0 };
const char HTTP_ERROR[] = { 0x0D, 0x0A, 'E', 'R', 'R', 'O', 'R', 0x0D, 0x0A,
		0x0D, 0x0A, 0 };
const char HTTP_OK[] = { 0x0D, 0x0A, 'O', 'K', 0x0D, 0x0A, 0 };
const char HTTP_RING[] = "#HTTPRING: ";

// <prof_id>,<http_status_code>,<content_type>,<data_size>
int http_check_error(int *retry) {

	char *token = NULL;
	int prof_id = 0;
	int http_status_code = 0;
	int data_size = 0;

	// Parse HTTPRING
	PARSE_FINDSTR_RET(token, HTTP_RING, UART_FAILED);

	PARSE_FIRSTVALUE(token, &prof_id, ",\n", UART_FAILED);
	PARSE_NEXTVALUE(token, &http_status_code, ",\n", UART_FAILED);
	PARSE_SKIP(token, ",\n", UART_FAILED); 	// Skip content_type string.
	PARSE_NEXTVALUE(token, &data_size, ",\n", UART_FAILED);

	log_appendf("HTTP ERROR %i [%d] - Data size %d ", prof_id, http_status_code,
			data_size);

	// Check for recoverable errors
	// Server didnt responded
	if (http_status_code == 200 && data_size == 0) {
		*retry = 1;
		lcd_printl(LINEC, "HTTP Server");
		lcd_printl(LINEH, "Empty response");
	}

	// TODO Find non recoverable errors
	return UART_SUCCESS;
}

int http_open_connection(int data_length) {
	char cmd[64];
	// Test post URL
	sprintf(cmd, "AT#HTTPSND=1,0,\"/coldtrace/intel/upload/\",%d,0\r\n", data_length);
	//sprintf(cmd, "AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v3/\",%d,0\r\n", data_length);

	// Wait for prompt
	uart_setHTTPPromptMode();
	if (uart_tx_waitForPrompt(cmd, TIMEOUT_HTTPSND_PROMPT)) {
		return UART_SUCCESS;
	}
	return uart_getTransactionState();
}

int http_get_configuration() {
	char szTemp[100];
	int uart_state = UART_FAILED;
	int retry = 1;
	int attempts = HTTP_COMMAND_ATTEMPTS;

	// #HTTPQRY � send HTTP GET, HEAD or DELETE request
	// Execution command performs a GET, HEAD or DELETE request to HTTP server.
	// Parameters:
	// <prof_id> - Numeric parameter indicating the profile identifier. Range: 0-2
	// <command>: Numeric parameter indicating the command requested to HTTP server:
	// 0 GET 1 HEAD 2 DELETE

	sprintf(szTemp, "AT#HTTPQRY=1,0,\"%s/%s/1/\"\r\n", CONFIGURATION_URL_PATH,
			g_pDevCfg->cfgIMEI);
	uart_tx_timeout(szTemp, 5000, 1);
	if (uart_getTransactionState() != UART_SUCCESS) {
		http_deactivate();
		return UART_FAILED;
	}

	// CME ERROR: 558 cannot resolve DN ?

	// Get the configration from the server
	// #HTTPRCV � receive HTTP server data

	uart_setCheckMsg(HTTP_OK, HTTP_ERROR);

	while (retry == 1 && --attempts > 0) {
		if (uart_tx_timeout("AT#HTTPRCV=1\r\n", 180000, 5) == UART_SUCCESS) {
			uart_state = uart_getTransactionState();
			if (uart_state == UART_SUCCESS) {
				retry = 0; 	// Found a configuration, lets parse it.
				config_process_configuration();
			}

			if (uart_state == UART_ERROR)
				http_check_error(&retry);  // Tries again
		}
	};

	http_deactivate();
	return uart_state; // TODO return was missing, is it necessary ?
}

int8_t http_post(char* postdata) {
	char cmd[64];
	http_enable();
	sprintf(cmd, "AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v3/\",%s,0\r\n", itoa_pad(strlen(postdata)));

	// Wait for prompt
	uart_setHTTPPromptMode();
	if (uart_tx_waitForPrompt(cmd, TIMEOUT_HTTPSND_PROMPT)) {
		uart_tx_timeout(postdata, TIMEOUT_HTTPSND, 1);
	}

	http_deactivate();
	return uart_getTransactionState();
}