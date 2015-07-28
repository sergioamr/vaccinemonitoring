#include "thermalcanyon.h"

const char ctrlZ[2] = { 0x1A, 0 };
const char ESC[2] = { 0x1B, 0 };

/*
 * AT Commands Reference Guide 80000ST10025a Rev. 9 2010-10-04
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

const char NETWORK_MODE_0[] = "Net disabled";
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
 CME ERROR: 15	SIM wrong
 CME ERROR: 30	No network service
 */

uint16_t CME_fatalErrors[] = { 10, 11, 12, 13, 15, 30 };

/*
 CMS ERROR: 30	Unknown subscriber
 CMS ERROR: 38	Network out of order
 */
uint16_t CMS_fatalErrors[] = { 10, 30, 38, 310 };

// Check against all the errors that could kill the SIM card operation
uint8_t modem_check_working_SIM() {
	int t = 0;

	if (!state_isSimOperational())
		return false;

	char token = '\0';
	uint16_t lastError = config_getSimLastError(&token);
	if (token == 'S')
		for (t = 0; t < sizeof(CMS_fatalErrors) / sizeof(uint16_t); t++)
			if (CMS_fatalErrors[t] == lastError) {
				state_SIM_not_operational();
				return false;
			}

	if (token == 'E')
		for (t = 0; t < sizeof(CME_fatalErrors) / sizeof(uint16_t); t++)
			if (CME_fatalErrors[t] == lastError) {
				state_SIM_not_operational();
				return false;
			}

	state_SIM_operational();
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

	uart_txf("+%s?", modem_getNetworkServiceCommand());

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

const char NETWORK_GPRS_COMMAND[] = "CGREG";
const char NETWORK_GSM_COMMAND[] = "CREG";

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
	if (g_pSysState->network_mode < 0 || g_pSysState->network_mode > 1)
		return NETWORK_GSM;

	return g_pSysState->network_mode;
}

void modem_setNetworkService(int service) {
	if (g_pSysState->network_mode == service)
		return;

	g_pSysState->network_mode = service;
	config_setLastCommand(COMMAND_SET_NETWORK_SERVICE);

	if (modem_connect_network(NETWORK_CONNECTION_ATTEMPTS) == UART_FAILED)
		return;
}

void modem_network_sequence() {
	uint8_t networkSwapped = 0;

	// Checks if the current sim is the selected one.
	modem_check_sim_active();

	config_setLastCommand(COMMAND_FAILOVER);
	if (modem_check_network() != UART_SUCCESS) {
		modem_swap_SIM();
		networkSwapped = 1;
		if (modem_check_network() != UART_SUCCESS) {
			return;
		}
	}

	if (g_pDevCfg->cfgUploadMode != MODE_GSM && state_isGPRS()
			&& http_enable() != UART_SUCCESS) {
		config_setLastCommand(COMMAND_FAILOVER_HTTP_FAILED);

		// This means we already checked the network
		if (networkSwapped) {
			http_deactivate();
			return;
		}

		modem_swap_SIM();
		if (modem_check_network() == UART_SUCCESS
				&& http_enable() != UART_SUCCESS) {
			state_failed_gprs(config_getSelectedSIM());
		}
	}

	http_deactivate();
}

#define MODEM_ERROR_SIM_NOT_INSERTED 10

int modem_connect_network(uint8_t attempts) {
	char token;
	int net_status = 0;
	int net_mode = 0;
	int tests = 0;
	int nsim = config_getSelectedSIM();

	/* PIN TO CHECK IF THE SIM IS INSERTED IS NOT CONNECTED, CPIN WILL NOT RETURN SIM NOT INSERTED */
	// Check if the SIM is inserted
	uart_tx("+CPBW=?");
	int uart_state = uart_getTransactionState();
	if (uart_state == UART_ERROR) {
		if (config_getSimLastError(&token) == MODEM_ERROR_SIM_NOT_INSERTED)
			return UART_ERROR;
	}

	if (!state_isSimOperational())
		return UART_ERROR;

	// enable network registration and location information unsolicited result code;
	// if there is a change of the network cell. +CGREG: <stat>[,<lac>,<ci>]

	config_setLastCommand(COMMAND_NETWORK_CONNECT);

	uart_txf("+%s=2", modem_getNetworkServiceCommand());
	do {
		if (modem_getNetworkStatus(&net_mode, &net_status) == UART_SUCCESS) {

			if (!g_iRunning || net_status != 1) {
				lcd_printf(LINEC, "SIM %d %s", nsim + 1,
						modem_getNetworkServiceText());
				lcd_printl(LINEH,
						(char *) modem_getNetworkStatusText(net_mode,
								net_status));
			}

			state_network_status(net_mode, net_status);

			// If we are looking for a network
			if ((net_mode == 1 || net_mode == 2)
					&& (net_status == NETWORK_STATUS_REGISTERED_HOME_NETWORK
							|| net_status == NETWORK_STATUS_REGISTERED_ROAMING)) {

				// TODO: If network is roaming don't connect
				state_network_success(nsim);

				// We tested more than once, lets show a nice we are connected message
				if (tests > 4)
					delay(HUMAN_DISPLAY_INFO_DELAY);
				return UART_SUCCESS;
			} else {
				if (g_pSysState->network_mode == NETWORK_GPRS) {
					state_failed_gprs(g_pDevCfg->cfgSIM_slot);
				} else {
					state_failed_gsm(g_pDevCfg->cfgSIM_slot);
				}
			}

			tests++;
		}

		lcd_progress_wait(NETWORK_CONNECTION_DELAY);

	} while (--attempts > 0);

	config_incLastCmd();
	return UART_FAILED;
}
;

int modem_disableNetworkRegistration() {
	return uart_txf("+%s=0\r\n", modem_getNetworkServiceCommand());
}

void modem_setNumericError(char errorToken, int16_t errorCode) {
	char szCode[16];

	char token;
	SIM_CARD_CONFIG *sim = config_getSIM();

	config_setLastCommand(COMMAND_SIM_ERROR);

	if (config_getSimLastError(&token) == errorCode)
		return;

	// Sim busy (we just have to try again, sim is busy... you know?)
	if (errorCode==14)
		return;

	// Activation failed
	if (errorCode==555)
		return;

	sprintf(szCode, "ERROR %d", errorCode);
	config_setSIMError(sim, errorToken, errorCode, szCode);

	// Check the error codes to figure out if the SIM is still functional
	modem_check_working_SIM();
	return;
}

static const char AT_ERROR[] = " ERROR: ";

// Used to ignore previsible errors like the SIM not supporting a command.
void modem_ignore_next_errors(int error) {
	uart.switches.b.ignoreError = error;
}

void modem_check_uart_error() {
	char *pToken1;
	char errorToken;

	int uart_state = uart_getTransactionState();
	if (uart_state != UART_ERROR)
		return;

	config_setLastCommand(COMMAND_UART_ERROR);

	pToken1 = strstr(uart_getRXHead(), (const char *) AT_ERROR);
	if (pToken1 != NULL) { // ERROR FOUND;
		char *error = (char *) (pToken1 + strlen(AT_ERROR));
		errorToken = *(pToken1 - 1);

		if (uart.switches.b.ignoreError == 0) {
			/*
#ifdef _DEBUG_OUTPUT
			if (errorToken=='S') {
				lcd_printl(LINEC, "SERVICE ERROR");
			} else {
				lcd_printl(LINEC, "MODEM ERROR");
			}
#endif
*/
			modem_setNumericError(errorToken, atoi(error));
		}
	}
}

// Makes sure that there is a network working
int8_t modem_check_network() {
	int res = UART_FAILED;
	int iSignal = 0;
	int service;

	config_setLastCommand(COMMAND_CHECK_NETWORK);

	// Check signal quality,
	// if it is too low we check if we
	// are actually connected to the network and fine
	service = modem_getNetworkService();

	iSignal = modem_getSignal();
	res = uart_getTransactionState();
	if (res == UART_SUCCESS) {
		state_setSignalLevel(iSignal);
	} else {
		g_pSysState->net_service[service].network_failures++;
		state_network_fail(config_getSelectedSIM(),
		NETWORK_ERROR_SIGNAL_FAILURE);
		log_appendf("[%d] NETDOWN %d", config_getSelectedSIM(),
				g_pSysState->net_service[service].network_failures);
	}

	return res;
}


int8_t modem_first_init() {
	int t = 0;
	int attempts = MODEM_CHECK_RETRY;
	int iSIM_Error = 0;

	config_setLastCommand(COMMAND_FIRST_INIT);

	lcd_printl(LINEC, "Power on");
	delay(500);

	//check Modem is powered on. Otherwise wait for a second
	while (!MODEM_ON && --attempts>0)
		delay(100);

	if (!MODEM_ON) {
		lcd_printl(LINEC, "Modem failed");
		lcd_printl(LINEE, "Power on");
	}

	uart_setOKMode();

	lcd_disable_verbose();
	//uart_tx_nowait(ESC); // Cancel any previous command in case we were reseted (not used anymore)
	uart_tx_timeout("AT", TIMEOUT_DEFAULT, 10); // Loop for OK until modem is ready
	lcd_enable_verbose();

	uint8_t nsims = SYSTEM_NUM_SIM_CARDS;

	/*
#ifdef _DEBUG
	nsims = 1;
#endif
*/

	for (t = 0; t < nsims; t++) {
		//force SIM 2 before we call swap. this ensures we start on SIM1
		g_pDevCfg->cfgSIM_slot = 1;
		modem_swap_SIM(); // Send hearbeat from SIM
		/*
		 * We have to check which parameters are fatal to disable the SIM
		 *
		 */

		if (!state_isSimOperational()) {
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
		lcd_printf(LINEE, "SIMS FAILED");
		break;
	}

	return MODEM_ON;
}

void modem_check_sim_active() {
	if (g_pDevCfg->cfgSIM_slot != g_pDevCfg->cfgSelectedSIM_slot) {
		modem_swap_SIM();
	}
}

int modem_swap_to_SIM(int sim) {
	if (g_pDevCfg->cfgSIM_slot != sim) {
		return modem_swap_SIM();
	}

	return UART_SUCCESS;
}

int modem_swap_SIM() {
	/*
#ifdef EXTREME_DEBUG
	log_appendf("SWP [%d]", config_getSelectedSIM());
#endif
*/
	int res = UART_FAILED;
	g_pDevCfg->cfgSIM_slot = !g_pDevCfg->cfgSIM_slot;
	g_pDevCfg->cfgSelectedSIM_slot = g_pDevCfg->cfgSIM_slot;

	lcd_printf(LINEC, "SIM %d", g_pDevCfg->cfgSIM_slot + 1);
	modem_init();
	modem_getExtraInfo();

	// Wait for the modem to be ready to send messages
#ifndef _DEBUG
	lcd_progress_wait(1000);
#endif

	// Just send the message if we dont have errors.
	if (state_isSimOperational()) {
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
	modem_ignore_next_errors(1); // Ignore 1 error since we know our sim cards dont support this command
	state = modem_parse_string("AT+CNUM?", "CNUM: \"", sim->cfgPhoneNum,
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

	uart_tx("+CGSN");
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

#define NETWORK_WAITING_TIME 5000 //10000
#define NET_ATTEMPTS 10


#if defined(CAPTURE_MCC_MNC) && defined(_DEBUG)
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
		uart_tx("#CSURVEXT=0");
		uart_state = uart_getTransactionState();

		if (uart_state == UART_SUCCESS) {

			// We just want only one full buffer. The data is on the first few characters of the stream
			uart_setNumberOfPages(1);
			uart_tx_timeout("AT#CSURV", TIMEOUT_CSURV, 10);// #CSURV - Network Survey
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
	}while (uart_state != UART_SUCCESS && attempts > 0);

	uart_setDefaultIntervalDivider();

	lcd_enable_verbose();
	lcd_progress_wait(10000); // Wait 10 seconds to make sure the modem finished transfering.
							  // It should be clear already but next transaction
}
#endif

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
	// Find the character before the next parse as it will be replaced by \0
	if (strchr(pToken1, '-') == NULL)
		negateTz = 1;
	PARSE_NEXTVALUE(pToken1, &tmTime.tm_sec, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(pToken1, &timeZoneOffset, delimiter, UART_FAILED);

	if (negateTz == 1) {
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
	uart_tx("+CPMS?");
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

	if (!state_isSimOperational())
		return;

	for (i = 0; i < NETWORK_PULLTIME_ATTEMPTS; i++) {
		res = uart_tx("AT+CCLK?");
		if (res == UART_SUCCESS)
			res = modem_parse_time(&g_tmCurrTime);

		if (res == UART_SUCCESS) {
			rtc_init(&g_tmCurrTime);
			config_update_system_time();
		} else {
			/*
#ifdef DEBUG_OUTPUT
			lcd_printf(LINEC, "WRONG DATE", config_getSelectedSIM());
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));
#endif
*/

			rtc_init(&g_pDevCfg->lastSystemTime);
			rtc_getlocal(&g_tmCurrTime);

			lcd_printf(LINEC, "LAST DATE");
			lcd_printf(LINEH, get_YMD_String(&g_tmCurrTime));
		}
	}
}

// Read command returns the selected PLMN selector <list> from the SIM/USIM
void modem_getPreferredOperatorList() {
	// List of networks for roaming
	uart_tx("+CPOL?");
}

void modem_init() {
	SIM_CARD_CONFIG *sim = config_getSIM();

	config_setLastCommand(COMMAND_MODEMINIT);
	config_getSelectedSIM(); // Init the SIM and check boundaries

	g_pSysState->system.switches.timestamp_on = 1;

	// Reset error states so we try to initialize the modem.
	// Reset the SIM flag if not operational
	config_reset_error(sim);

	uart_tx("AT"); // Display OK
	uart_tx("E0");

	// GPIO [PIN, DIR, MODE]
	// Execution command sets the value of the general purpose output pin
	// GPIO<pin> according to <dir> and <mode> parameter.
	uart_tx("#SIMDET=0"); // Enable automatic pin sim detection

	if (config_getSelectedSIM() != 1) {
		//enable SIM A (slot 1)
		uart_tx_timeout("#GPIO=2,0,1", TIMEOUT_GPO, 5); // Sim 1 PWR enable - First command always has a chance of timeout
		uart_tx("#GPIO=4,1,1"); // Sim 2 PWR enable
		uart_tx("#GPIO=3,0,1"); // Sim select
	} else {
		//enable SIM B (slot 2)
		uart_tx_timeout("#GPIO=2,1,1", TIMEOUT_GPO, 5); // Sim 1 PWR enable
		uart_tx("#GPIO=4,0,1"); // Sim 2 PWR enable
		uart_tx("#GPIO=3,1,1"); // Sim select
	}

	uart_tx_timeout("#SIMDET=1", MODEM_TX_DELAY2, 10); // Disable sim detection. Is it not plugged in hardware?

	uart_tx("+CMEE=1"); // Set command enables/disables the report of result code:
	uart_tx("#CMEEMODE=1"); // This command allows to extend the set of error codes reported by CMEE to the GPRS related error codes.
	uart_tx("#AUTOBND=2"); // Set command enables/disables the automatic band selection at power-on.  if automatic band selection is enabled the band changes every about 90 seconds through available bands until a GSM cell is found.

	//uart_tx("AT+CPMS=\"ME\",\"ME\",\"ME\"");

	// Check if there are pending messages in the SMS queue

	// We have to wait for the network to be ready, it will take some time. In debug we just wait on connect. It will generate a SIM BUSY error (14)
#ifndef _DEBUG
	lcd_progress_wait(2000);
#endif
	// After autoband it could take up to 90 seconds for the bands trial and error.
	// So we have to wait for the modem to be ready.

	modem_setNetworkService(NETWORK_GSM);

	// Wait until connnection and registration is successful. (Just try NETWORK_CONNECTION_ATTEMPTS) network could be definitly down or not available.
	modem_setNetworkService(NETWORK_GPRS);

	uart_tx("+CGSMS=1"); // Will try to use GPRS for texts otherwise will use GSM
	uart_tx("#NITZ=15,1");   // #NITZ - Network Timezone

	uart_tx("+CTZU=1"); //  4 - software bi-directional with filtering (XON/XOFF)
	uart_tx("&K4");		// [Flow Control - &K]
	uart_tx("&P0"); // [Default Reset Full Profile Designation - &P] Execution command defines which full profile will be loaded on startup.
	uart_tx("&W0"); // [Store Current Configuration - &W] 			 Execution command stores on profile <n> the complete configuration of the	device

	modem_getSMSCenter();
	modem_getSignal();

	uart_tx("+CSDH=1"); // Show Text Mode Parameters - +CSDH  			 Set command controls whether detailed header information is shown in text mode (+CMGF=1) result codes
	uart_tx_timeout("+CMGF=?", MODEM_TX_DELAY2, 5); // set sms format to text mode

#if defined(CAPTURE_MCC_MNC) && defined(_DEBUG)
	modem_survey_network();
#endif

#ifdef POWER_SAVING_ENABLED
	uart_tx("+CFUN=5"); //full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);

	uart_tx("+CFUN?");//full modem functionality with power saving enabled (triggered via DTR)
	delay(MODEM_TX_DELAY1);
#endif

	if (check_address_empty(sim->iMaxMessages))
		modem_set_max_messages();

	modem_pull_time();

	// Have to call twice to guarantee a genuine result
	modem_getSignal();

	modem_getOwnNumber();

	http_setup();
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
