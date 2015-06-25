/*
 * modem.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include <msp430.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "driverlib.h"
#include "config.h"
#include "modem.h"
#include "common.h"
#include "modem_uart.h"
#include "MMC.h"
#include "pmm.h"
#include "lcd.h"
#include "globals.h"
#include "sms.h"
#include "timer.h"
#include "time.h"
#include "rtc.h"
#include "http.h"
#include "temperature.h"
#include "fatdata.h"
#include "state_machine.h"
#include "main_system.h"

char ctrlZ[2] = { 0x1A, 0 };
char ESC[2] = { 0x1B, 0 };

/*
 * AT Commands Reference Guide 80000ST10025a Rev. 9 � 2010-10-04
 *
 * Read command reports the <mode> and <stat> parameter values in the format:
 * +CREG: <mode>,<stat>[,<Lac>,<Ci>]
 *
 * <mode>
 * 0 - disable network registration unsolicited result code (factory default)
 * 1 - enable network registration unsolicited result code
 * 2 - enable network registration unsolicited result code with network Cell identification data
 *
 * If <mode>=1, network registration result code reports:
 * +CREG: <stat> where <stat>
 *  0 - not registered, ME is not currently searching a new operator to register to
 *  1 - registered, home network
 *  2 - not registered, but ME is currently searching a new operator to register to
 *  3 - registration denied
 *  4 -unknown
 *  5 - registered, roaming
 *
 *  If <mode>=2, network registration result code reports:
 *  +CREG: <stat>[,<Lac>,<Ci>]
 *  where:
 *  <Lac> - Local Area Code for the currently registered on cell
 *  <Ci> - Cell Id for the currently registered on cell
 *
 *
 *  Note: <Lac> and <Ci> are reported only if <mode>=2 and the mobile is registered on some network cell.
 */

const char NETWORK_MODE_0[] = "Network disabled";
const char * const NETWORK_STATUS[6] = { "initializing", "connected",
		"searching net", "reg denied", "unknown state", "roaming net" };
const char NETWORK_MODE_2[] = "Registered";
const char NETWORK_UNKNOWN_STATUS[] = "Unknown";
const char * const NETWORK_SERVICE_TEXT[2] = { "GSM", "GPRS" };

/*
 CME ERROR: 10	SIM not inserted
 CME ERROR: 11	SIM PIN required
 CME ERROR: 12	SIM PUK required
 CME ERROR: 13	SIM failure
 CME ERROR: 14	SIM busy
 CME ERROR: 15	SIM wrong
 CME ERROR: 30	No network service
 */

uint16_t CME_fatalErrors[] = { 10, 11, 12, 13, 14, 15, 30 };

/*
 CMS ERROR: 30	Unknown subscriber
 CMS ERROR: 38	Network out of order
 */
uint16_t CMS_fatalErrors[] = { 30, 38 };

// Check against all the errors that could kill the SIM card operation
uint8_t modem_isSIM_Operational() {
	int t = 0;

	char token = '\0';
	uint16_t lastError = config_getSimLastError(&token);
	if (token == 'S')
		for (t = 0; t < sizeof(CMS_fatalErrors) / sizeof(uint16_t); t++)
			if (CMS_fatalErrors[t] == lastError) {
				config_SIM_not_operational();
				return false;
			}

	if (token == 'E')
		for (t = 0; t < sizeof(CME_fatalErrors) / sizeof(uint16_t); t++)
			if (CME_fatalErrors[t] == lastError) {
				config_SIM_not_operational();
				return false;
			}

	config_SIM_operational();
	return true;
}

const char *modem_getNetworkStatusText(int mode, int status) {

	if (mode == 0) {
		state_setNetworkStatus(NETWORK_UNKNOWN_STATUS);
		return NETWORK_MODE_0;
	}

	if (status >= 0 && status < 7) {
		state_setNetworkStatus(NETWORK_STATUS[status]);
		return NETWORK_STATUS[status];
	}

	state_setNetworkStatus(NETWORK_UNKNOWN_STATUS);
	return NETWORK_UNKNOWN_STATUS;
}

int modem_getNetworkStatus(int *mode, int *status) {

	char *pToken1 = NULL;
	int uart_state = 0;
	int verbose;
	char command_result[16];

	SIM_CARD_CONFIG *sim = config_getSIM();

	*mode = -1;
	*status = -1;

	verbose = lcd_getVerboseMode();
	lcd_disable_verbose();

	sprintf(command_result, "+%s: ", modem_getNetworkServiceCommand());

	uart_txf("AT+%s?\r\n", modem_getNetworkServiceCommand());

	lcd_setVerboseMode(verbose);

	uart_state = uart_getTransactionState();
	if (uart_state == UART_SUCCESS) {

		PARSE_FINDSTR_RET(pToken1, command_result, UART_FAILED);

		PARSE_FIRSTVALUE(pToken1, mode, ",\n", UART_FAILED);
		PARSE_NEXTVALUE(pToken1, status, ",\n", UART_FAILED);

		// Store the current network status to know if we are connected or not.
		sim->networkMode = *mode;
		sim->networkStatus = *status;

		return UART_SUCCESS;
	}
	return UART_FAILED;
}

/**********************************************************************************************************************/
/* Network cellular service */
/**********************************************************************************************************************/

char NETWORK_GPRS_COMMAND[] = "CGREG";
char NETWORK_GSM_COMMAND[] = "CREG";

const char inline *modem_getNetworkServiceCommand() {
	if (g_pSysState->network_mode == NETWORK_GPRS)
		return NETWORK_GPRS_COMMAND;

	return NETWORK_GSM_COMMAND;
}

const char inline *modem_getNetworkServiceText() {
	switch (g_pSysState->network_mode) {
		case NETWORK_GSM:
			return NETWORK_SERVICE_TEXT[NETWORK_GSM];
		case NETWORK_GPRS:
			return NETWORK_SERVICE_TEXT[NETWORK_GPRS];
	}

	return NETWORK_UNKNOWN_STATUS;
}

int modem_getNetworkService() {
	if (g_pSysState->network_mode<0 || g_pSysState->network_mode>1)
		return NETWORK_GSM;

	return g_pSysState->network_mode;
}

void modem_setNetworkService(int service) {
	if (g_pSysState->network_mode != service) {
		g_pSysState->network_mode = service;
		modem_connect_network(NETWORK_CONNECTION_ATTEMPTS);
	}
}

void modem_run_failover_sequence() {
	if (modem_check_network() != UART_SUCCESS) {
		modem_swap_SIM();
		log_appendf("[%d] SIMSWAP", config_getSelectedSIM());
		if (modem_check_network() != UART_SUCCESS) {
			g_pSysState->lastTransMethod = NONE;
		}
	}

	if (http_enable() != UART_SUCCESS) {
		modem_swap_SIM();
		log_appendf("[%d] SIMSWAP", config_getSelectedSIM());
		if (modem_check_network() == UART_SUCCESS &&
				http_enable() != UART_SUCCESS) {
			if (g_pDevCfg->cfgSIM_slot == 0) {
				g_pSysState->lastTransMethod = SMS_SIM1;
			} else {
				g_pSysState->lastTransMethod = SMS_SIM2;
			}
		}
	} else {
		if (g_pDevCfg->cfgSIM_slot == 0) {
			g_pSysState->lastTransMethod = HTTP_SIM1;
		} else {
			g_pSysState->lastTransMethod = HTTP_SIM2;
		}
	}

	http_deactivate();
}

int modem_connect_network(uint8_t attempts) {

	int net_status = 0;
	int net_mode = 0;
	int tests = 0;
	int nsim = config_getSelectedSIM();

	// enable network registration and location information unsolicited result code;
	// if there is a change of the network cell. +CGREG: <stat>[,<lac>,<ci>]

	uart_txf("AT+%s=2\r\n", modem_getNetworkServiceCommand());
	do {
		if (modem_getNetworkStatus(&net_mode, &net_status) == UART_SUCCESS) {

			if (!g_iRunning || net_status != 1) {
				lcd_printf(LINEC, "SIM %d NET %s", nsim + 1, modem_getNetworkServiceText());
				lcd_printl(LINEH,
						(char *) modem_getNetworkStatusText(net_mode,
								net_status));
			}

			state_network_status(net_mode, net_status);

			// If we are looking for a network
			if ((net_mode == 1 || net_mode == 2)
					&& (net_status == NETWORK_STATUS_REGISTERED_HOME_NETWORK
							|| net_status == NETWORK_STATUS_REGISTERED_ROAMING)) {

				state_network_success(nsim);

				// We tested more than once, lets show a nice we are connected message
				if (tests > 2)
					delay(HUMAN_DISPLAY_INFO_DELAY);
				return UART_SUCCESS;
			} else {
				state_network_fail(nsim, STATE_CONNECTION);
			}

			tests++;
		}

		lcd_progress_wait(NETWORK_CONNECTION_DELAY);
	} while (--attempts > 0);

	return UART_FAILED;
}
;

int modem_disableNetworkRegistration() {
	return uart_txf("AT+%s=0\r\n", modem_getNetworkServiceCommand());
}

// Not used if we activate the error handling
uint16_t modem_parse_error(const char *error) {
	uint16_t errorNum = NO_ERROR;
	if (!strcmp(error, "SIM not inserted\r\n")) {
		SIM_CARD_CONFIG *sim = config_getSIM();
		config_setSIMError(sim, 'E', ERROR_SIM_NOT_INSERTED, error);
		return ERROR_SIM_NOT_INSERTED;
	}

	_NOP();
	return errorNum;
}

void modem_setNumericError(char errorToken, int16_t errorCode) {
	char szCode[16];
	char szToken[2];
	char token;
	if (config_getSimLastError(&token) == errorCode)
		return;

	if (errorCode==4) {
		strcpy(szCode, "Not supported");
	} else {
		sprintf(szCode, "ERROR %d", errorCode);
	}

	SIM_CARD_CONFIG *sim = config_getSIM();
	config_setSIMError(sim, errorToken, errorCode, szCode);

	// Check the error codes to figure out if the SIM is still functional
	modem_isSIM_Operational();

	szToken[0]=errorToken;  // Minimal SPRINTF support
	szToken[1]=0;

	log_appendf("SIM %d CMD[%s] CM%s ERROR %d", config_getSelectedSIM() + 1, &modem_lastCommand[0], szToken,
			errorCode);

	return;
}

static const char AT_ERROR[] = " ERROR: ";

uint8_t g_iUartIgnoreError = 0;
void modem_ignore_next_errors(int errors) {
	g_iUartIgnoreError = errors;
}

void modem_check_uart_error() {
	char *pToken1;
	char errorToken;

	int uart_state = uart_getTransactionState();
	if (uart_state != UART_ERROR)
		return;

	pToken1 = strstr(uart_getRXHead(), (const char *) AT_ERROR);
	if (pToken1 != NULL) { // ERROR FOUND;
		char *error = (char *) (pToken1 + strlen(AT_ERROR));
		errorToken = *(pToken1 - 1);

		if (g_iUartIgnoreError != 0) {
			g_iUartIgnoreError--;
			log_appendf("ERROR: SIM %d CMD[%s] ERROR %s", config_getSelectedSIM(), &modem_lastCommand[0],
					error);
#ifndef _DEBUG
			if (errorToken=='S') {
				lcd_printl(LINEC, "SERVICE ERROR");
			} else {
				lcd_printl(LINEC, "MODEM ERROR");
			}
#endif
			lcd_printl(LINEE, error);
		}
		modem_setNumericError(errorToken, atoi(error));
	}

	lcd_progress_wait(HUMAN_DISPLAY_INFO_DELAY);
}

// Makes sure that there is a network working
int8_t modem_check_network() {
	int res = UART_FAILED;
	int iSignal = 0;

	// Check signal quality,
	// if it is too low we check if we
	// are actually connected to the network and fine

	iSignal = modem_getSignal();
	res = uart_getTransactionState();
	if (res == UART_SUCCESS)
		state_setSignalLevel(iSignal);
	else
		state_network_fail(config_getSelectedSIM(),
		NETWORK_ERROR_SIGNAL_FAILURE);
	return res;
}

int8_t modem_first_init() {

	int t = 0;
	int iIdx;
	int iStatus = 0;
	int iSIM_Error = 0;

	lcd_printl(LINEC, "Modem power on");
	delay(500);

	//check Modem is powered on
	for (iIdx = 0; iIdx < MODEM_CHECK_RETRY; iIdx++) {
		if ((P4IN & BIT0) == 0) {
			iStatus |= MODEM_POWERED_ON;
			break;
		} else {
			iStatus &= ~MODEM_POWERED_ON;
			delay(100);
		}
	}

	if (iStatus & MODEM_POWERED_ON) {
		uart_setOKMode();

		lcd_disable_verbose();
		uart_tx_nowait(ESC); // Cancel any previous command in case we were reseted
		uart_tx_timeout("AT\r\n", TIMEOUT_DEFAULT, 10); // Loop for OK until modem is ready
		lcd_enable_verbose();

		uint8_t nsims = SYSTEM_NUM_SIM_CARDS;

#ifdef _DEBUG
		nsims = 1;
#endif

		for (t = 0; t < nsims; t++) {
			modem_swap_SIM(); // Send hearbeat from SIM
			/*
			 * We have to check which parameters are fatal to disable the SIM
			 *
			 */

			if (!modem_isSIM_Operational()) {
				log_appendf("ERROR: SIM %d FAILED [%s]",
						config_getSelectedSIM(), config_getSIM()->simLastError);
				lcd_printf(LINEE, config_getSIM()->simLastError);
				iSIM_Error++;
			}
		}

		// One or more of the sims had a catastrofic failure on init, set the device
		switch (iSIM_Error) {
		case 1:
			for (t = 0; t < SYSTEM_NUM_SIM_CARDS; t++)
				if (config_getSIMError(t) == NO_ERROR) {
					if (config_getSelectedSIM() != t) {
						g_pDevCfg->cfgSIM_slot = t;
						g_pDevCfg->cfgSelectedSIM_slot = t;
						modem_init();
					}
				}
			break;
		case 2:
			lcd_printf(LINEC, "BOTH SIMS FAILED ");
			log_appendf("SIMS FAILED ON INIT ");
			delay(HUMAN_DISPLAY_ERROR_DELAY);
			break;
		}

	} else {
		lcd_printl(LINEE, "Failed Power On");
	}

	return iStatus;
}

void modem_check_sim_active() {
	if (g_pDevCfg->cfgSIM_slot != g_pDevCfg->cfgSelectedSIM_slot) {
		modem_swap_SIM();
	}
}

int modem_swap_to_SIM(int sim) {
	if (g_pDevCfg->cfgSelectedSIM_slot != sim) {
		return modem_swap_SIM();
	}

	return UART_SUCCESS;
}

int modem_swap_SIM() {
	int res = UART_FAILED;
	g_pDevCfg->cfgSIM_slot = !g_pDevCfg->cfgSIM_slot;
	g_pDevCfg->cfgSelectedSIM_slot = g_pDevCfg->cfgSIM_slot;

	lcd_printf(LINEC, "Activate SIM: %d", g_pDevCfg->cfgSIM_slot + 1);
	modem_init();
	modem_getExtraInfo();

	// Wait for the modem to be ready to send messages
#ifndef _DEBUG
	lcd_progress_wait(1000);
#endif

	// Just send the message if we dont have errors.
	if (modem_isSIM_Operational()) {
		sms_send_heart_beat(); // Neccessary?
		res = UART_SUCCESS;
	} else {
		res = UART_FAILED;
	}

	return res;
}

const char COMMAND_RESULT_CSQ[] = "CSQ: ";

int modem_getSignal() {
	char *token;
	int iSignalLevel = 0;

	if (uart_tx_timeout("AT+CSQ\r\n", TIMEOUT_CSQ, 1) != UART_SUCCESS)
		return 0;

	PARSE_FINDSTR_RET(token, COMMAND_RESULT_CSQ, UART_FAILED);
	PARSE_FIRSTVALUE(token, &iSignalLevel, ",\n", UART_FAILED);

	//pToken2 = strtok(&pToken1[5], ",");
	state_setSignalLevel(iSignalLevel);
	return iSignalLevel;
}

int8_t modem_parse_string(char *cmd, char *response, char *destination,
		uint16_t size) {
	char *token;
	int8_t uart_state;

	uart_tx_timeout(cmd, MODEM_TX_DELAY1, 1);

	uart_state = uart_getTransactionState();
	if (uart_state == UART_SUCCESS) {
		PARSE_FINDSTR_RET(token, response, UART_FAILED);
		PARSE_FIRSTSTRING(token, destination, size, "\"", UART_FAILED);
	}

	return uart_state;
}

// Reading the Service Center Address to use as message gateway
// http://www.developershome.com/sms/cscaCommand.asp
// Get service center address; format "+CSCA: address,address_type"

int8_t modem_getSMSCenter() {
	SIM_CARD_CONFIG *sim = config_getSIM();
	return modem_parse_string("AT+CSCA?\r\n", "CSCA: \"", sim->cfgSMSCenter,
	GW_MAX_LEN + 1);
	// added for SMS Message center number to be sent in the heart beat
}

int8_t modem_getOwnNumber() {
	SIM_CARD_CONFIG *sim = config_getSIM();
	int8_t state;
	modem_ignore_next_errors(1);
	state = modem_parse_string("AT+CNUM?\r\n", "CNUM: \"", sim->cfgPhoneNum,
	GW_MAX_LEN + 1);
	modem_ignore_next_errors(0);
	return state;
}

uint8_t validateIMEI(char *IMEI) {
	int t;
	uint8_t len = strlen(IMEI);

	for (t = 0; t < len; t++) {
		if (IMEI[t] == '\r' || IMEI[t] == '\n') {
			IMEI[t] = 0;
			break;
		}
		if (IMEI[t] < '0' || IMEI[t] > '9')
			return 0;
	}

	len = strlen(IMEI);
	if (len < IMEI_MIN_LEN)
		return 0;

	return 1;
}

void modem_getIMEI() {
	// added for IMEI number//
	char IMEI[IMEI_MAX_LEN + 1];
	char *token = NULL;

	uart_tx("AT+CGSN\r\n");
	memset(IMEI, 0, sizeof(IMEI));

	token = strstr(uart_getRXHead(), "OK");
	if (token == NULL)
		return;

	strncpy(IMEI, uart_getRXHead() + 2, IMEI_MAX_LEN);
	if (!validateIMEI(IMEI)) {
		return;
	}

	if (check_address_empty(g_pDevCfg->cfgIMEI[0])) {
		strcpy(g_pDevCfg->cfgIMEI, IMEI);
	}

	// Lets check if we have the right IMEI from the modem, otherwise we flash it again into config.
	if (memcmp(IMEI, g_pDevCfg->cfgIMEI, IMEI_MAX_LEN) != 0) {
		strcpy(g_pDevCfg->cfgIMEI, IMEI);
	}

}

void modem_getExtraInfo() {
	modem_getIMEI();
	delay(50);
}

// 2 minutes timeout by the TELIT documentation
#define MAX_CSURV_TIME 120000

#ifdef _DEBUG
#define NETWORK_WAITING_TIME 5000
#define NET_ATTEMPTS 10
#else
#define NETWORK_WAITING_TIME 10000
#define NET_ATTEMPTS 10
#endif

void modem_survey_network() {

	char *pToken1;
	int attempts = NET_ATTEMPTS;
	int uart_state;

	SIM_CARD_CONFIG *sim = config_getSIM();

	if (sim->iCfgMCC != 0 && sim->iCfgMNC != 0)
		return;

	lcd_disable_verbose();

	uart_setDelayIntervalDivider(64);  // 120000 / 64

	do {
		if (attempts != NET_ATTEMPTS)
			lcd_printf(LINEC, "MCC RETRY %d   ", NET_ATTEMPTS - attempts);
		else
			lcd_printl(LINEC, "MCC DISCOVER");

		// Displays info for the only serving cell
		uart_tx("AT#CSURVEXT=0\r\n");
		uart_state = uart_getTransactionState();

		if (uart_state == UART_SUCCESS) {

			// We just want only one full buffer. The data is on the first few characters of the stream
			uart_setNumberOfPages(1);
			uart_tx_timeout("AT#CSURV\r\n", TIMEOUT_CSURV, 10); // #CSURV - Network Survey
			// Maximum timeout is 2 minutes

			//Execution command allows to perform a quick survey through channels
			//belonging to the band selected by last #BND command issue, starting from
			//channel <s> to channel <e>. If parameters are omitted, a full band scan is
			//performed.

			uart_state = uart_getTransactionState();
			if (uart_state != UART_SUCCESS) {
				lcd_printl(LINE2, "NETWORK BUSY");
				delay(NETWORK_WAITING_TIME);
			} else {

				pToken1 = strstr(uart_getRXHead(), "ERROR");
				if (pToken1 != NULL) {
					uart_state = UART_ERROR;
					lcd_printl(LINE2, "FAILED");
					delay(1000);
				} else {
					pToken1 = strstr(uart_getRXHead(), "mcc:");
					if (pToken1 != NULL)
						sim->iCfgMCC = atoi(pToken1 + 5);

					pToken1 = strstr((const char *) pToken1, "mnc:");
					if (pToken1 != NULL)
						sim->iCfgMNC = atoi(pToken1 + 5);

					if (sim->iCfgMCC > 0 && sim->iCfgMNC > 0) {
						lcd_printl(LINEC, "SUCCESS");
						lcd_printf(LINE2, "MCC %d MNC %d", sim->iCfgMCC,
								sim->iCfgMNC);
					}
				}
			}
		}

		attempts--;
	} while (uart_state != UART_SUCCESS && attempts > 0);

	uart_setDefaultIntervalDivider();

	lcd_enable_verbose();
	lcd_progress_wait(10000); // Wait 10 seconds to make sure the modem finished transfering.
							  // It should be clear already but next transaction
}

const char COMMAND_RESULT_CCLK[] = "+CCLK: \"";

int modem_parse_time(struct tm* pTime) {
	struct tm tmTime;
	char* pToken1 = NULL;
	char* delimiter = "/,:-\"+";
	int timeZoneOffset = 0;
	uint8_t negateTz = 0;

	if (!pTime)
		return UART_FAILED;

	PARSE_FINDSTR_RET(pToken1, COMMAND_RESULT_CCLK, UART_FAILED);

	memset(&tmTime, 0, sizeof(tmTime));
	//string format "yy/MM/dd,hh:mm:ss+-zz"
	PARSE_FIRSTVALUE(pToken1, &tmTime.tm_year, delimiter, UART_FAILED);
	tmTime.tm_year += 2000;

	PARSE_NEXTVALUE(pToken1, &tmTime.tm_mon, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_mday, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_hour, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_min, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_sec, delimiter, UART_FAILED);
	if (strchr(pToken1, '-') == NULL) negateTz = 1;
	PARSE_NEXTVALUE(pToken1, &timeZoneOffset, delimiter, UART_FAILED);

	if(negateTz == 1) {
		_tz.timezone = 0 - ((timeZoneOffset >> 2) * 3600);
	} else {
		_tz.timezone = (timeZoneOffset >> 2) * 3600;
	}

	if (tmTime.tm_year < 2015 || tmTime.tm_year > 2200)
		return UART_FAILED;

	memcpy(pTime, &tmTime, sizeof(tmTime));
	return UART_SUCCESS;
}

int8_t modem_set_max_messages() {
	int8_t uart_state;
	char *token;
	char memory[16];
	int storedmessages = 0;
	//check if messages are available
	uart_tx("AT+CPMS?\r\n");
	uart_state = uart_getTransactionState();
	if (uart_state == UART_SUCCESS) {
		PARSE_FINDSTR_RET(token, "CPMS: \"", UART_FAILED);
		PARSE_FIRSTSTRING(token, memory, 16, "\"", UART_FAILED);
		PARSE_NEXTVALUE(token, &storedmessages, ",\n", UART_FAILED);
		PARSE_NEXTVALUE(token, &config_getSIM()->iMaxMessages, ",\n",
				UART_FAILED);
	}

	if (storedmessages != 0)
		_NOP();
	return uart_state;
}

void modem_pull_time() {
	int i;
	int res = UART_FAILED;
	for (i = 0; i < NETWORK_PULLTIME_ATTEMPTS; i++) {
		res = uart_tx("AT+CCLK?\r\n");
		if (res == UART_SUCCESS)
			res = modem_parse_time(&g_tmCurrTime);

		if (res == UART_SUCCESS) {
			rtc_init(&g_tmCurrTime);
			config_update_system_time();
		} else {
			lcd_printf(LINEC, "WRONG DATE SIM %d ", config_getSelectedSIM());
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));

			rtc_init(&g_pDevCfg->lastSystemTime);
			rtc_getlocal(&g_tmCurrTime);

			lcd_printf(LINEC, "LAST DATE ");
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));
		}
	}
}

// Read command returns the selected PLMN selector <list> from the SIM/USIM
void modem_getPreferredOperatorList() {
	// List of networks for roaming
	uart_tx("AT+CPOL?\r\n");
}

void modem_init() {

#if defined _DEBUG && defined DEBUG_SAVE_COMMAND
	config_setLastCommand(COMMAND_MODEMINIT);
#endif

	config_getSelectedSIM(); // Init the SIM and check boundaries

	SIM_CARD_CONFIG *sim = config_getSIM();

	// Reset error state so we try to initialize the modem.
	config_reset_error(sim);

	uart_tx("AT\r\n"); // Display OK
	uart_tx("ATE0\r\n");

	// GPIO [PIN, DIR, MODE]
	// Execution command sets the value of the general purpose output pin
	// GPIO<pin> according to <dir> and <mode> parameter.
	uart_tx("AT#SIMDET=0\r\n"); // Enable automatic pin sim detection

#ifdef ENABLE_SIM_SLOT
	if (config_getSelectedSIM() != 1) {
		//enable SIM A (slot 1)
		uart_tx_timeout("AT#GPIO=2,0,1\r\n", TIMEOUT_GPO, 5); // Sim 1 PWR enable - First command always has a chance of timeout
		uart_tx("AT#GPIO=4,1,1\r\n"); // Sim 2 PWR enable
		uart_tx("AT#GPIO=3,0,1\r\n"); // Sim select
	} else {
		//enable SIM B (slot 2)
		uart_tx_timeout("AT#GPIO=2,1,1\r\n", TIMEOUT_GPO, 5); // Sim 1 PWR enable
		uart_tx("AT#GPIO=4,0,1\r\n"); // Sim 2 PWR enable
		uart_tx("AT#GPIO=3,1,1\r\n"); // Sim select
	}
#endif

	uart_tx_timeout("AT#SIMDET=1\r\n", MODEM_TX_DELAY2, 10); // Disable sim detection. Is it not plugged in hardware?

	uart_tx("AT+CMEE=1\r\n"); // Set command enables/disables the report of result code:
	uart_tx("AT#CMEEMODE=1\r\n"); // This command allows to extend the set of error codes reported by CMEE to the GPRS related error codes.
	uart_tx("AT#AUTOBND=2\r\n"); // Set command enables/disables the automatic band selection at power-on.  if automatic band selection is enabled the band changes every about 90 seconds through available bands until a GSM cell is found.
	uart_tx("AT+CGSMS=2\r\n"); // Will try to use GPRS for texts otherwise will use GSM

	//uart_tx("AT+CPMS=\"ME\",\"ME\",\"ME\"");

	// Check if there are pending messages in the SMS queue

	// We have to wait for the network to be ready, it will take some time. In debug we just wait on connect.
#ifndef _DEBUG
	lcd_progress_wait(2000);
#endif
	// After autoband it could take up to 90 seconds for the bands trial and error.
	// So we have to wait for the modem to be ready.

	modem_setNetworkService(NETWORK_GSM);

	// Wait until connnection and registration is successful. (Just try NETWORK_CONNECTION_ATTEMPTS) network could be definitly down or not available.
	modem_setNetworkService(NETWORK_GPRS);

	uart_tx("AT#NITZ=15,1\r\n");   // #NITZ - Network Timezone

	uart_tx("AT+CTZU=1\r\n"); //  4 - software bi-directional with filtering (XON/XOFF)
	uart_tx("AT&K4\r\n");		// [Flow Control - &K]
	uart_tx("AT&P0\r\n"); // [Default Reset Full Profile Designation - &P] Execution command defines which full profile will be loaded on startup.
	uart_tx("AT&W0\r\n"); // [Store Current Configuration - &W] 			 Execution command stores on profile <n> the complete configuration of the	device

	modem_getSMSCenter();
	modem_getSignal();

	uart_tx("AT+CSDH=1\r\n"); // Show Text Mode Parameters - +CSDH  			 Set command controls whether detailed header information is shown in text mode (+CMGF=1) result codes
	uart_tx_timeout("AT+CMGF=?\r\n", MODEM_TX_DELAY2, 5); // set sms format to text mode

#if defined(CAPTURE_MCC_MNC) && defined(_DEBUG)
	modem_survey_network();
#endif

#ifdef POWER_SAVING_ENABLED
	uart_tx("AT+CFUN=5\r\n"); //full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);

	uart_tx("AT+CFUN?\r\n");//full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);
#endif

	if (check_address_empty(sim->iMaxMessages))
		modem_set_max_messages();

	modem_pull_time();

	// Have to call twice to guarantee a genuine result
	modem_getSignal();

#ifndef _DEBUG
	modem_getOwnNumber();
#endif

	http_setup();

	// Check if the network/sim card works
	modem_isSIM_Operational();
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
