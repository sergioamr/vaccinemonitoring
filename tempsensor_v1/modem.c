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
#include "uart.h"
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

char ctrlZ[2] = { 0x1A, 0 };
char ESC[2] = { 0x1B, 0 };

// Parsing macros helpers
#define PARSE_FIRSTVALUE(token, var, delimiter, error) token = strtok(token, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

#define PARSE_NEXTVALUE(token, var, delimiter, error) token = strtok(NULL, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

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
 *  Note: <Lac> and <Ci> are reported only if <mode>=2 and the mobile is registered on some network cell.
 */

const char NETWORK_MODE_0[] = "Network disabled";
const char * const NETWORK_STATUS[6] = { "not connected", "connected home",
		"searching net", "reg denied", "unknown", "roaming" };
const char NETWORK_MODE_2[] = "Registered";
const char NETWORK_UNKNOWN_STATUS[] = "Unknown";

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

	if (mode == 0)
		return NETWORK_MODE_0;

	if (status >= 0 && status < 7)
		return NETWORK_STATUS[status];

	return NETWORK_UNKNOWN_STATUS;
}

const char COMMAND_RESULT_CGREG[] = "+CGREG: ";

int modem_getNetworkStatus(int *mode, int *status) {

	char *pToken1 = NULL;
	int uart_state = 0;

	SIM_CARD_CONFIG *sim = config_getSIM();

	*mode = -1;
	*status = -1;

	uart_tx("AT+CGREG?\r\n");

	uart_state = uart_getTransactionState();
	if (uart_state == UART_SUCCESS) {
		pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx],
				(const char *) COMMAND_RESULT_CGREG);
		if (pToken1 == NULL)
			return UART_FAILED;

		pToken1 += sizeof(COMMAND_RESULT_CGREG) - 1; // Jump the header

		PARSE_FIRSTVALUE(pToken1, mode, ",\n", UART_FAILED);
		PARSE_NEXTVALUE(pToken1, status, ",\n", UART_FAILED);

		// Store the current network status to know if we are connected or not.
		sim->networkMode = *mode;
		sim->networkStatus = *status;

		return UART_SUCCESS;
	}
	return UART_FAILED;
}

int modem_connect_network(uint8_t attempts) {

	int net_status = 0;
	int net_mode = 0;
	int tests = 0;

	// enable network registration and location information unsolicited result code;
	// if there is a change of the network cell. +CGREG: <stat>[,<lac>,<ci>]

	uart_tx("AT+CGREG=2\r\n");
	do {
		if (modem_getNetworkStatus(&net_mode, &net_status) == UART_SUCCESS) {

			lcd_printf(LINEC, "SIM %d status", config_getSelectedSIM() + 1);
			lcd_printl(LINEH,
					(char *) modem_getNetworkStatusText(net_mode, net_status));

			// If we are looking for a network
			if ((net_mode == 1 || net_mode == 2)
					&& (net_status == NETWORK_STATUS_REGISTERED_HOME_NETWORK
							|| net_status == NETWORK_STATUS_REGISTERED_ROAMING)) {

				// We tested more than once, lets show a nice we are connected message
				if (tests > 0)
					delay(HUMAN_DISPLAY_INFO_DELAY);
				return UART_SUCCESS;
			}

			tests++;
		}

		lcd_progress_wait(NETWORK_CONNECTION_DELAY);
	} while (--attempts > 0);

	return UART_FAILED;
}
;

int modem_disableNetworkRegistration() {
	return uart_tx("AT+CGREG=0\r\n");
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
	char token;
	if (config_getSimLastError(&token) == errorCode)
		return;

	sprintf(szCode, "ERROR %d", errorCode);
	SIM_CARD_CONFIG *sim = config_getSIM();
	config_setSIMError(sim, errorToken, errorCode, szCode);

	// Check the error codes to figure out if the SIM is still functional
	modem_isSIM_Operational();

	log_appendf("SIM %d CM%c ERROR %d", config_getSelectedSIM() + 1, errorToken,
			errorCode);
	return;
}

static const char AT_ERROR[] = " ERROR: ";

void modem_check_uart_error() {
	char *pToken1;
	char errorToken;

	int uart_state = uart_getTransactionState();
	if (uart_state != UART_ERROR)
		return;

	pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx],
			(const char *) AT_ERROR);
	if (pToken1 != NULL) { // ERROR FOUND;
		char *error = (char *) (pToken1 + strlen(AT_ERROR));

		log_appendf("ERROR: SIM %d cmd[%s]", config_getSelectedSIM(), error);
		errorToken = *(pToken1 - 1);
#ifndef _DEBUG
		if (errorToken=='S') {
			lcd_printl(LINEC, "SERVICE ERROR");
		} else {
			lcd_printl(LINEC, "MODEM ERROR");
		}
#endif
		lcd_printl(LINEE, error);
		modem_setNumericError(errorToken, atoi(error));
	}

	delay(5000);
}

int8_t modem_first_init() {

	int t = 0;
	int iIdx;
	int iStatus = 0;
	int iSIM_Error = 0;

	lcd_printl(LINEC, "Modem POWER ON");

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

		uint8_t nsims = NUM_SIM_CARDS;

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

				lcd_printf(LINEC, "ERROR INIT SIM %d ",
						config_getSelectedSIM() + 1);
				lcd_printf(LINEE, config_getSIM()->simLastError);

				iSIM_Error++;
			}
		}

#ifdef _DEBUG
		if (http_setup() == UART_SUCCESS) {
			http_get_configuration();
		}
#endif

		// One or more of the sims had a catastrofic failure on init, set the device
		switch (iSIM_Error) {
		case 1:
			for (t = 0; t < NUM_SIM_CARDS; t++)
				if (config_getSIMError(t) == NO_ERROR) {
					if (config_getSelectedSIM() != t) {
						g_pDeviceCfg->cfgSIM_slot = t;
						modem_init();
					}
				}
			break;
		case 2:
			lcd_printf(LINEC, "BOTH SIMS FAILED ");
			log_appendf("ERROR: BOTH SIMS FAILED ON INIT ");
			delay(HUMAN_DISPLAY_ERROR_DELAY);
			break;
		}

		//heartbeat
		for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
			memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);
			digital_amp_to_temp_string(ADCvar[iIdx], &Temperature[iIdx][0],
					iIdx);
		}
	} else {
		lcd_printl(LINEE, "Failed Power On");
	}

	return iStatus;
}

void modem_swap_SIM() {
	config_incLastCmd();
	g_pDeviceCfg->cfgSIM_slot = !g_pDeviceCfg->cfgSIM_slot;

	lcd_printf(LINEC, "Activate SIM: %d", g_pDeviceCfg->cfgSIM_slot + 1);
	modem_init();
	modem_getExtraInfo();

	// Wait for the modem to be ready to send messages
#ifndef _DEBUG
	lcd_progress_wait(1000);
#endif

	// Just send the message if we dont have errors.
	if (modem_isSIM_Operational()) {
		sms_send_heart_beat();
	}
}

void modem_checkSignal() {

	config_incLastCmd();

	if (uart_tx_timeout("AT+CSQ\r\n", TIMEOUT_CSQ, 1) != UART_SUCCESS)
		return;

	uart_rx_cleanBuf(ATCMD_CSQ, ATresponse, sizeof(ATresponse));
	if (ATresponse[0] != 0) {
		iSignalLevel = strtol(ATresponse, 0, 10);
		if ((iSignalLevel < NETWORK_DOWN_SS)
				|| (iSignalLevel > NETWORK_MAX_SS)) {
			g_iStatus |= NETWORK_DOWN;
		} else {
			g_iStatus &= ~NETWORK_DOWN;
		}
	}
}

void modem_getSMSCenter() {

	SIM_CARD_CONFIG *sim = config_getSIM();
	config_incLastCmd();

	// Reading the Service Center Address to use as message gateway
	// http://www.developershome.com/sms/cscaCommand.asp
	// Get service center address; format "+CSCA: address,address_type"

	// added for SMS Message center number to be sent in the heart beat
	uart_tx("AT+CSCA?\r\n");

	if (uart_rx_cleanBuf(ATCMD_CSCA, ATresponse,
			sizeof(ATresponse))==UART_SUCCESS) {

		memcpy(sim->cfgSMSCenter, ATresponse, strlen(ATresponse));
	}
}

void modem_getIMEI() {
	// added for IMEI number//
	char IMEI_OK = false;
	config_incLastCmd();

	uart_tx("AT+CGSN\r\n");
	if (uart_rx_cleanBuf(ATCMD_CGSN, ATresponse,
			sizeof(ATresponse))==UART_SUCCESS) {

		if (memcmp(ATresponse, "INVALID", strlen("INVALID"))) {
			//copy valid IMEI to FRAM
			IMEI_OK = true;
		} else {
			lcd_printl(LINEE, "IMEI Error");
		}
	}

	if (!IMEI_OK)
		return;

	if (check_address_empty(g_pDeviceCfg->cfgIMEI[0])) {
		strcpy(g_pDeviceCfg->cfgIMEI, ATresponse);
	}

	// Lets check if we have the right IMEI from the modem, otherwise we flash it again into config.
	if (memcmp(ATresponse, g_pDeviceCfg->cfgIMEI, 15) != 0) {
		strcpy(g_pDeviceCfg->cfgIMEI, ATresponse);
	}

}

void modem_getExtraInfo() {
	modem_getIMEI();
	delay(100);
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
	config_incLastCmd();

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

				pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "ERROR");
				if (pToken1 != NULL) {
					uart_state = UART_ERROR;
					lcd_printl(LINE2, "FAILED");
					delay(1000);
				} else {
					pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx],
							"mcc:");
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

int modem_parse_time(char* pDatetime, struct tm* pTime) {
	struct tm tmTime;
	char* pToken1 = NULL;
	char* delimiter = "/,:-\"";

	if (!pDatetime || !pTime)
		return UART_FAILED;

	pToken1 = strstr((const char *) pDatetime,
			(const char *) COMMAND_RESULT_CCLK);
	if (pToken1 == NULL)
		return UART_FAILED;

	pToken1 += sizeof(COMMAND_RESULT_CCLK) - 1;

	memset(&tmTime, 0, sizeof(tmTime));
	//string format "yy/MM/dd,hh:mm:ss:zz"
	PARSE_FIRSTVALUE(pToken1, &tmTime.tm_year, delimiter, UART_FAILED);
	tmTime.tm_year += 2000;

	PARSE_NEXTVALUE(pToken1, &tmTime.tm_mon, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_mday, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_hour, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_min, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_sec, delimiter, UART_FAILED);

	if (tmTime.tm_year < 2015 || tmTime.tm_year > 2200)
		return UART_FAILED;

	memcpy(pTime, &tmTime, sizeof(tmTime));
	return UART_SUCCESS;
}

void modem_set_max_messages() {
	//check if messages are available
	uart_tx("AT+CPMS?\r\n");
	uart_rx_cleanBuf(ATCMD_CPMS_MAX, ATresponse, sizeof(ATresponse));
	config_getSIM()->iMaxMessages = atoi(ATresponse);
}

void modem_pull_time() {
	int i;
	int res = UART_FAILED;
	for (i = 0; i < MAX_TIME_ATTEMPTS; i++) {
		res = uart_tx("AT+CCLK?\r\n");
		if (res == UART_SUCCESS)
			res = modem_parse_time((char *) &RXBuffer[RXHeadIdx],
					&g_tmCurrTime);

		if (res == UART_SUCCESS) {
			rtc_init(&g_tmCurrTime);
			config_update_system_time();
		} else {
			lcd_printf(LINEC, "WRONG DATE SIM %d ", config_getSelectedSIM());
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));

			rtc_init(&g_pDeviceCfg->lastSystemTime);
			rtc_get(&g_tmCurrTime);

			lcd_printf(LINEC, "LAST DATE ");
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));

		}

		if (g_tmCurrTime.tm_year != 0) {
			// Day has changed so save the new date TODO keep trying until date is set. Call function ONCE PER DAY
			g_iCurrDay = g_tmCurrTime.tm_mday;
			break;
		}
	}
}

// Read command returns the selected PLMN selector <list> from the SIM/USIM
void modem_getPreferredOperatorList() {
	// List of networks for roaming
	uart_tx("AT+CPOL?\r\n");
}

void modem_init() {

	config_setLastCommand(COMMAND_MODEMINIT);

	if (config_getSelectedSIM() > 1) {
		// Memory not initialized
		g_pDeviceCfg->cfgSIM_slot = 0;
	}

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

	// We have to wait for the network to be ready, it will take some time. In debug we just wait on connect.
#ifndef _DEBUG
	lcd_progress_wait(1000);
#endif
	// After autoband it could take up to 90 seconds for the bands trial and error.
	// So we have to wait for the modem to be ready.

	// Wait until connnection and registration is successful. (Just try NETWORK_CONNECTION_ATTEMPTS) network could be definitly down or not available.
	modem_connect_network(NETWORK_CONNECTION_ATTEMPTS);

	uart_tx("AT#NITZ=1\r\n");   // #NITZ - Network Timezone

	uart_tx("AT+CTZU=1\r\n"); //  4 - software bi-directional with filtering (XON/XOFF)
	uart_tx("AT&K4\r\n");		// [Flow Control - &K]
	uart_tx("AT&P0\r\n"); // [Default Reset Full Profile Designation - &P] Execution command defines which full profile will be loaded on startup.
	uart_tx("AT&W0\r\n"); // [Store Current Configuration - &W] 			 Execution command stores on profile <n> the complete configuration of the	device

	modem_getSMSCenter();
	modem_checkSignal();

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
	modem_checkSignal();

	lcd_print("Checking GPRS");
	/// added for gprs connection..//
	g_iSignal_gprs = http_post_gprs_connection_status(GPRS);
	g_iGprs_network_indication = http_post_gprs_connection_status(GSM);

	// Disable echo from modem
	uart_tx("ATE0\r\n");

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
