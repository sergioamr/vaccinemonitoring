/*
 * config.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */
#define CONFIG_C_

// Reads the configuration from a file in disk
//#define RUN_CALIBRATION

#include "thermalcanyon.h"
#include "calibration.h"
#include "time.h"
#include "stringutils.h"
#include "hardware_buttons.h"
#include "time.h"
#include "events.h"
#include "state_machine.h"
#include "ff.h"
#include "fatdata.h"

#ifdef USE_MININI
#include "minIni.h"
FRESULT config_read_ini_file();
#endif

#ifdef _DEBUG
#define DEBUG_SEND_CONFIG
#endif

// Setup mode in which we are at the moment
// Triggered by the Switch 3 button
int8_t g_iSystemSetup = -1;

#define RUN_CALIBRATION

/************************** BEGIN CONFIGURATION MEMORY ****************************************/

#pragma SET_DATA_SECTION(".ConfigurationArea")
CONFIG_DEVICE g_ConfigDevice; // configuration of the device, APN, gateways, etc.
CONFIG_SYSTEM g_ConfigSystem; // is system initialized, number of runs, and overall stats
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".calibration_globals")
CONFIG_CALIBRATION g_ConfigCalibration;
#pragma SET_DATA_SECTION()

// Since we use the area outside the 16 bits we have to use the large memory model.
// The compiler gives a warning accessing the structure directly and it can fail. Better accessing it through the pointer.
CONFIG_DEVICE *g_pDevCfg = &g_ConfigDevice;
CONFIG_SYSTEM* g_pSysCfg = &g_ConfigSystem;
CONFIG_CALIBRATION *g_pCalibrationCfg = &g_ConfigCalibration;

// Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.

/************************** END CONFIGURATION MEMORY ****************************************/

// Checks if a memory address is not initialized (used for strings)
uint8_t check_address_empty(uint8_t mem) {
	return ((uint8_t) mem == 0xFF || (uint8_t) mem == 0);
}

void calibrate_device() {

#ifdef RUN_CALIBRATION
	config_setLastCommand(COMMAND_CALIBRATION);

	g_iLCDVerbose = VERBOSE_BOOTING;         // Booting is not completed

	main_calibration();
#endif
	g_pSysCfg->calibrationFinished = 1;
	//reset the board by issuing a SW BOR
#ifdef RUN_CALIBRATION

	// Only reset the device in release mode
#ifndef _DEBUG
	PMM_trigBOR();
	while(1);
#endif
#endif
}

void config_reset_error(SIM_CARD_CONFIG *sim) {
	sim->simErrorToken = '\0';
	sim->simErrorState = NO_ERROR;
	state_SIM_operational();
}

void config_display_config() {
	int t;

	lcd_printf(LINEC, "SMS Gateway");
	lcd_printf(LINEH, g_pDevCfg->cfgGatewaySMS);

	lcd_printf(LINEC, "Gateway IP");
	lcd_printf(LINEH, g_pDevCfg->cfgGatewayIP);

	for (t = 0; t < 2; t++) {
		lcd_printf(LINEC, "APN %d", t + 1);
		lcd_printf(LINEH, g_pDevCfg->SIM[t].cfgAPN);
	}

	lcd_display_config();
}

//TODO This should go to the state machine

void config_setSIMError(SIM_CARD_CONFIG *sim, char errorToken, uint16_t errorID,
		const char *error) {

	char CM[4] = "CMX";

	if (error == NULL || sim == NULL)
		return;

	sim->simErrorState = errorID;
	sim->simErrorToken = errorToken;

	// 69 - "Requested facility not implemented"
	// This cause indicates that the network is unable to provide the requested short message service.

	if (errorID == 0)
		return;

	CM[2] = errorToken;
	ini_gets(CM, itoa_nopadding(errorID), error, sim->simLastError,
			sizeof(sim->simLastError), "errors.ini");

	if (errorID == 69)
		state_setSMS_notSupported(sim);

	state_sim_failure(sim);
	modem_check_working_SIM();

	event_LCD_turn_on();
}

// Returns the current structure containing the info for the current SIM selected
uint16_t inline config_getSIMError(int slot) {
	return g_pDevCfg->SIM[slot].simErrorState;
}

// Returns the current structure containing the info for the current SIM selected
SIM_CARD_CONFIG *config_getSIM() {
	int8_t slot = g_pDevCfg->cfgSIM_slot;
	if (slot < 0 || slot > 1)
		g_pDevCfg->cfgSIM_slot = 0;

	return &g_pDevCfg->SIM[slot];
}

uint8_t config_getSelectedSIM() {
	if (g_pDevCfg->cfgSIM_slot < 0 || g_pDevCfg->cfgSIM_slot > 1)
		g_pDevCfg->cfgSIM_slot = 0;

	return g_pDevCfg->cfgSIM_slot;
}

uint8_t config_is_SIM_configurable(int simSlot) {
	SIM_CARD_CONFIG *sim;

	if (simSlot >= SYSTEM_NUM_SIM_CARDS) {
		return 0;
	}

	sim = &g_pDevCfg->SIM[simSlot];

	if (sim->cfgAPN[0] == '\0' && sim->cfgPhoneNum[0] == '\0') {
		return 0;
	}

	return 1;
}

uint16_t config_getSimLastError(char *charToken) {

	uint8_t slot = g_pDevCfg->cfgSIM_slot;
	if (charToken != NULL)
		*charToken = g_pDevCfg->SIM[slot].simErrorToken;

	return g_pDevCfg->SIM[slot].simErrorState;
}

void config_SafeMode() {
	_NOP();
}

// Debugging functionality by storing last command runs

// Stores what was the last command run and what time
void config_setLastCommand(uint16_t lastCmd) {
	g_pSysCfg->lastCommand = lastCmd;
#ifdef ___CHECK_STACK___
	checkStack();
#endif
#ifdef EXTREME_DEBUG
	if (g_pDevCfg->cfg.logs.commmands) {
		log_appendf("CMD [%d]", lastCmd);
	}
#endif
}

void config_incLastCmd() {
	g_pSysCfg->lastCommand++;
#ifdef EXTREME_DEBUG
	if (g_pDevCfg->cfg.logs.commmands)
	config_setLastCommand(g_pSysCfg->lastCommand);
#endif
}

// Runs the system in configuration/calibration mode
void config_reconfigure() {
	g_pSysCfg->memoryInitialized = 0xFF;
	PMM_trigBOR();
	while (1)
		;
}

// Send back data after an SMS request
void config_send_configuration(char *number) {
#ifdef DEBUG_SEND_CONFIG
	TEMP_ALERT_PARAM *alert;
	BATT_POWER_ALERT_PARAM *power;
	uint16_t maxSize = 0;
	char *msg = getSMSBufferHelper();
	char *temp = getStringBufferHelper(&maxSize);
	int i;

	zeroString(msg);
	for (i = 0; i < SYSTEM_NUM_SENSORS; i++) {
		alert = &g_pDevCfg->stTempAlertParams[i];
		sprintf(temp, "%s(C%dm H%dm tC %d tH %d)\r\n", SensorName[i],
				(int) alert->maxSecondsCold / 60,
				(int) alert->maxSecondsHot / 60, (int) alert->threshCold,
				(int) alert->threshHot);
		strcat(msg, temp);
	}
	sms_send_message_number(number, msg);

	power = &g_pDevCfg->stBattPowerAlertParam;
	sprintf(msg, "Power %d(%d mins) \nBatt (%d mins thresd %d)\n"
			"\nIntervals Mins:\nSampling %d\nUpload %d\nReboot %d\nConfig %d",
			power->enablePowerAlert, (int) power->minutesPower,
			(int) power->minutesBattThresh, (int) power->battThreshold,

			(int) g_pDevCfg->sIntervalsMins.sampling,
			(int) g_pDevCfg->sIntervalsMins.upload,
			(int) g_pDevCfg->sIntervalsMins.systemReboot,
			(int) g_pDevCfg->sIntervalsMins.configurationFetch);
	sms_send_message_number(number, msg);

	sprintf(msg, "GATEWAY [%s]\r\n"
			"SIM1APN [%s]\r\n"
			"LASTERR [%s]\r\n"
			"SIM2APN [%s]\r\n"
			"LASTERR [%s]\r\n", g_pDevCfg->cfgGatewaySMS,
			g_pDevCfg->SIM[0].cfgAPN, g_pDevCfg->SIM[0].simLastError,

			g_pDevCfg->SIM[1].cfgAPN, g_pDevCfg->SIM[1].simLastError);

	sms_send_message_number(number, msg);
	releaseStringBufferHelper();
#endif
}

char g_bServiceMode = false;

void config_init() {

	memset(&g_sEvents, 0, sizeof(g_sEvents));

	// Check if the user is pressing the service mode
	// Service Button was pressed during bootup. Rerun calibration
	if (switch_check_service_pressed()) {
		g_bServiceMode = true;
		lcd_printf(LINEC, "Service Mode");
		delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
		g_pSysCfg->calibrationFinished = 0;
	} else if (!check_address_empty(g_pSysCfg->memoryInitialized)) {
// Data structure changed, something failed.
// Check firmware version?
		if (g_pSysCfg->configStructureSize != sizeof(CONFIG_SYSTEM)) {
			config_SafeMode();
		}

// Problem calibrating? Try again.
		if (!g_pSysCfg->calibrationFinished) {
			calibrate_device();
			return;
		}
		g_pSysCfg->numberConfigurationRuns++;

		return; // Memory was initialized, we are fine here.
	}

// Init Config InfoA
	memset(g_pDevCfg, 0, sizeof(CONFIG_DEVICE));

// Setup InfoA config data
	g_pDevCfg->cfgSIM_slot = 0;
	g_pDevCfg->cfgSelectedSIM_slot = 0;

	strcpy(g_pDevCfg->cfgReportSMS, REPORT_PHONE_NUMBER); // HTTP server nextleaf

	strcpy(g_pDevCfg->cfgGatewayIP, NEXLEAF_DEFAULT_SERVER_IP); // HTTP server nextleaf
	strcpy(g_pDevCfg->cfgGatewaySMS, NEXLEAF_SMS_GATEWAY); // Gateway to nextleaf
	strcpy(g_pDevCfg->cfgConfig_URL, CONFIGURATION_URL_PATH);
	strcpy(g_pDevCfg->cfgUpload_URL, DATA_UPLOAD_URL_PATH);

	config_setSIMError(&g_pDevCfg->SIM[0], '+', NO_ERROR, "1 OK");
	config_setSIMError(&g_pDevCfg->SIM[1], '+', NO_ERROR, "2 OK");

	strcpy(g_pDevCfg->SIM[0].cfgAPN, NEXLEAF_DEFAULT_APN);
	strcpy(g_pDevCfg->SIM[1].cfgAPN, NEXLEAF_DEFAULT_APN);

// Init System internals

// Setup internal system counters and checks
	memset(g_pSysCfg, 0, sizeof(CONFIG_SYSTEM));

// First run
	g_pSysCfg->numberConfigurationRuns = 1;
	g_pSysState->lastSeek = 0;

// Value to check to make sure the structure is still the same size;
	g_pSysCfg->configStructureSize = sizeof(CONFIG_SYSTEM);
	g_pSysCfg->memoryInitialized = 1;

// Set the date and time of compilation as firmware version
	strcpy(g_pSysCfg->firmwareVersion, "v(" __DATE__ ")");
#ifdef _DEBUG_COUNT_BUFFERS
	g_pSysCfg->maxSamplebuffer = 0;

	g_pSysCfg->maxRXBuffer = 0;
	g_pSysCfg->maxTXBuffer = 0;
#endif
	config_default_configuration();
#ifdef USE_MININI
	config_read_ini_file();
#endif

	lcd_printf(LINEC, "CONFIG");
	lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
#ifndef _DEBUG
			delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
#endif

// First initalization, calibration code.
	calibrate_device();
}

void config_update_system_time() {

// Gets the current time and stores it in FRAM
	rtc_getlocal(&g_tmCurrTime);
	memcpy(&g_pDevCfg->lastSystemTime, &g_tmCurrTime, sizeof(g_tmCurrTime));
}

// http://54.241.2.213/coldtrace/uploads/multi/v3/358072043112124/1/
// $ST2,7,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,600,1,600,15,$ST1,919243100142,both,airtelgprs.com,10,1,0,0,$EN

const char CHUNK_ST1[5] = "$ST1";
const char CHUNK_ST2[5] = "$ST2";
const char CHUNK_ST3[5] = "$ST3";
const char CHUNK_END[3] = "$EN";
const char delimiter[2] = ",";

int config_count_delims(char* string, char delim) {
	int i = 0, count = 0, stringLen = strlen(string);

	if (string[0] != '$')
		return count;

	i++;
	while (i < RX_LEN && i < stringLen && string[i] != '\0' && string[i] != '$') {
		if (string[i] == delim)
			count++;
		i++;
	}

	return count;
}

/*
 NEW config sync $ST1,<GATEWAY NUMBER>,<GPRS/GSM/BOTH>,
 <COLDTRACE IP ADDRESS>,<APN1>,<APN2>,<UPLOADURL>,<CONFIGURL>,
 <UPLOAD INTERVAL>,<SAMPLE INTERVAL>,<SIM CARD>,<ALARM STATE>,$EN

 $ST1,+447482787262,SMS,54.241.2.213,giffgaff.com,giffgaff.com,/coldtrace/uploads/multi/v4/,/coldtrace/configuration/ct5/v2/,45,5,0,0,$EN
 */
int config_parse_configuration_ST1(char *token) {
	int iCnt = 0;
	int tempValue = 0;
	INTERVAL_PARAM *pInterval;
	SIM_CARD_CONFIG *sim = config_getSIM();

	//SYSTEM_SWITCHES *system; //pointer to system_switches struct
	char uploadMode[6];

	if (config_count_delims(token, ',') != ST1_NUM_PARAMS) {
#ifdef _DEBUG
		log_append_("ST1 WRONG FORMAT");
#endif
		return UART_SUCCESS;
	}

	memset(uploadMode, 0, sizeof(uploadMode));

	config_setLastCommand(COMMAND_PARSE_CONFIG_ST1);
	lcd_printl(LINEH, (char *) CHUNK_ST1);

	// Skip $ST1,
	PARSE_FIRSTSKIP(token, delimiter, UART_FAILED);

	// gateway
	PARSE_NEXTSTRING(token, g_pDevCfg->cfgGatewaySMS,
			sizeof(g_pDevCfg->cfgGatewaySMS), delimiter, UART_FAILED); // GATEWAY NUM

	//upload_mode
	PARSE_NEXTSTRING(token, uploadMode, sizeof(uploadMode), delimiter,
			UART_FAILED); //parse network type
	//(0: force gprs, 1: force sms, 2: DEFAULT: failover mode
	if (!strncmp(uploadMode, "GPRS", sizeof(uploadMode)))
		g_pDevCfg->cfgUploadMode = MODE_GPRS;
	else if (!strncmp(uploadMode, "SMS", sizeof(uploadMode)))
		g_pDevCfg->cfgUploadMode = MODE_GSM;
	else
		//default BOTH
		g_pDevCfg->cfgUploadMode = MODE_GSM + MODE_GPRS;

	//ip address
	PARSE_NEXTSTRING(token, g_pDevCfg->cfgGatewayIP,
			sizeof(g_pDevCfg->cfgGatewayIP), delimiter, UART_FAILED); // GATEWAY NUM

	//APN
	//get APN for both sim
	PARSE_NEXTSTRING(token, g_pDevCfg->SIM[0].cfgAPN,
			sizeof(g_pDevCfg->SIM[0].cfgAPN), delimiter, UART_FAILED); //APN1

	PARSE_NEXTSTRING(token, g_pDevCfg->SIM[1].cfgAPN,
			sizeof(g_pDevCfg->SIM[1].cfgAPN), delimiter, UART_FAILED); //APN2

	//upload URL
	PARSE_NEXTSTRING(token, g_pDevCfg->cfgUpload_URL,
			sizeof(g_pDevCfg->cfgUpload_URL), delimiter, UART_FAILED);

#ifdef _DEBUG
	strcpy(g_pDevCfg->cfgUpload_URL, "/coldtrace/intel/upload/");
#endif

	//config URL
	PARSE_NEXTSTRING(token, g_pDevCfg->cfgConfig_URL,
			sizeof(g_pDevCfg->cfgConfig_URL), delimiter, UART_FAILED);

	//load intervals
	pInterval = &g_pDevCfg->sIntervalsMins;
	//upload interval
	PARSE_NEXTVALUE(token, &pInterval->upload, delimiter, UART_FAILED);

	//sample interval
	PARSE_NEXTVALUE(token, &pInterval->sampling, delimiter, UART_FAILED);

	//sim_force
	//(0,1,2)... apply directly to config param using reference
	PARSE_NEXTVALUE(token, &g_pDevCfg->cfgSIM_force, delimiter, UART_FAILED);

	// reset_alarm
	PARSE_NEXTVALUE(token, &tempValue, delimiter, UART_FAILED); // Reset alert
	if (tempValue > 0) {
		state_clear_alarm_state();
		for (iCnt = 0; iCnt < SYSTEM_NUM_SENSORS; iCnt++) {
			//reset the alarm
			state_reset_sensor_alarm(iCnt);
		}
	}

	//TODO add disable timer config?
	// disable/enable buzzer
	// Currently set up to take 0 or 1 for off or on, can be changed if needed
	/*
	 system = &g_pSysState->system; //switches initialized in system_states
	 PARSE_NEXTVALUE(token, &tempValue , delimiter, UART_FAILED); //read a value
	 system->switches.buzzer_disabled = tempValue; //set to buzzer disabled param
	 */

	// TODO CHECK IF THE SIM CARD IS OPPERATIONAL
	g_pDevCfg->cfgSelectedSIM_slot = tempValue;

#ifdef _DEBUG
	log_append_("ST1 OK");
#endif
	return UART_SUCCESS;
}

// Example:
// $ST2,2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,23,1,22,21,$EN
// NUM SENSORS { <SENSOR LOW DELAY> <SENSOR LOW TEMP> <SENSOR HIGH DELAY> <SENSOR HIGH TEMP> }
// 23,1,22,21 <POWER OUTAGE DELAY> <POWER ALARM ACTIVATE> <LOW BATTERY DELAY> <LOW BATTERY LEVEL ALARM>

int config_parse_configuration_ST2(char *token) {
	int i = 0;

	TEMP_ALERT_PARAM *pAlertParams;
	BATT_POWER_ALERT_PARAM *pBattPower;

	if (config_count_delims(token, ',') != ST2_NUM_PARAMS) {
#ifdef _DEBUG
		log_append_("ST2 WRONG FORMAT");
#endif
		return UART_SUCCESS;
	}

	config_setLastCommand(COMMAND_PARSE_CONFIG_ST2);
	lcd_printl(LINEH, CHUNK_ST2);

	// Skip $ST2,
	PARSE_FIRSTSKIP(token, delimiter, UART_FAILED);

	// Temperature configuration for each sensor
	while (i < SYSTEM_NUM_SENSORS) {
		pAlertParams = &g_pDevCfg->stTempAlertParams[i];
		PARSE_NEXTVALUE(token, &pAlertParams->maxSecondsCold, delimiter,
				UART_FAILED);
		pAlertParams->maxSecondsCold = MINUTES_(pAlertParams->maxSecondsCold); // We work in seconds internally
		PARSE_NEXTVALUE(token, &pAlertParams->threshCold, delimiter,
				UART_FAILED);

		PARSE_NEXTVALUE(token, &pAlertParams->maxSecondsHot, delimiter,
				UART_FAILED);
		pAlertParams->maxSecondsHot = MINUTES_(pAlertParams->maxSecondsHot); // We work in seconds internally
		PARSE_NEXTVALUE(token, &pAlertParams->threshHot, delimiter, UART_FAILED);
		i++;
	}

	// Battery config info.
	// 23,1,22,21 <POWER OUTAGE DELAY> <POWER ALARM ACTIVATE> <LOW BATTERY DELAY> <LOW BATTERY LEVEL ALARM>

	pBattPower = &g_pDevCfg->stBattPowerAlertParam;
	PARSE_NEXTVALUE(token, &pBattPower->minutesPower, delimiter, UART_FAILED);

	PARSE_NEXTVALUE(token, &pBattPower->enablePowerAlert, delimiter,
			UART_FAILED);

	PARSE_NEXTVALUE(token, &pBattPower->minutesBattThresh, delimiter,
			UART_FAILED);

	PARSE_NEXTVALUE(token, &pBattPower->battThreshold, delimiter, UART_FAILED);

#ifdef _DEBUG
	log_append_("ST2 OK");
#endif

	return UART_SUCCESS;
}

int config_parse_configuration_ST3(char *token) {
	if (config_count_delims(token, ',') != ST3_NUM_PARAMS) {
#ifdef _DEBUG
		log_append_("ST3 WRONG FORMAT");
#endif
		return UART_SUCCESS;
	}

	config_setLastCommand(COMMAND_PARSE_CONFIG_ST3);
	lcd_printl(LINEH, CHUNK_ST3);

	// Skip $ST3,
	PARSE_FIRSTSKIP(token, delimiter, UART_FAILED);

#ifdef _DEBUG
	log_append_("ST3 OK");
#endif

	return UART_SUCCESS;
}

int config_parse_configuration(char *msg) {
	char *tokenEND, *tokenST3, *tokenST2, *tokenST1;
	INTERVAL_PARAM *pInterval = &g_pDevCfg->sIntervalsMins;

	config_setLastCommand(COMMAND_PARSE_CONFIG_ONLINE);

	lcd_printf(LINEC, "Parsing Config");

	fat_save_config(msg);

	// Try to see if we have an end for the config
	tokenEND = strstr((const char *) msg, "$EN");
	if (tokenEND == NULL)
		return UART_FAILED;

	tokenST1 = strstr((const char *) msg, CHUNK_ST1);
	tokenST2 = strstr((const char *) msg, CHUNK_ST2);
	tokenST3 = strstr((const char *) msg, CHUNK_ST3);

	if (tokenST1 != NULL)
		if (config_parse_configuration_ST1(tokenST1) == UART_FAILED)
			return UART_FAILED;

	if (tokenST2 != NULL)
		if (config_parse_configuration_ST2(tokenST2) == UART_FAILED)
			return UART_FAILED;

	if (tokenST3 != NULL)
		if (config_parse_configuration_ST3(tokenST3) == UART_FAILED)
			return UART_FAILED;

	g_pDevCfg->cfgServerConfigReceived = 1;

	event_setInterval_by_id_secs(EVT_SUBSAMPLE_TEMP,
			MINUTES_(pInterval->sampling));

	event_setInterval_by_id_secs(EVT_SAVE_SAMPLE_TEMP,
			MINUTES_(pInterval->sampling));

	event_setInterval_by_id_secs(EVT_UPLOAD_SAMPLES,
			MINUTES_(pInterval->upload));

	config_incLastCmd();
#ifdef CONFIG_SAVE_IN_PROGRESS
	config_save_ini();
#endif

	// Order the system to send the config later on.
	g_sEvents.defer.command.display_config = 1;
	return UART_SUCCESS;
}

int config_process_configuration() {
	char *token;
#ifdef _DEBUG
	log_append_("cfg process");
#endif
// FINDSTR uses RXBuffer - There is no need to initialize the data to parse.
	PARSE_FINDSTR_RET(token, HTTP_INCOMING_DATA, UART_FAILED);
	return config_parse_configuration((char *) uart_getRXHead());
}

#define SECTION_SERVER "SERVER"
#define SECTION_INTERVALS "INTERVALS"
#define SECTION_LOGS "LOGS"

#ifdef USE_MININI

// This functionality doesnt work yet
#ifdef CONFIG_SAVE_IN_PROGRESS
void config_save_ini() {
	long n;
	INTERVAL_PARAM *intervals;

	//f_rename(CONFIG_INI_FILE, "thermal.old");
	lcd_printf(LINEC, "SAVING CONFIG");

	n = ini_puts(SECTION_SERVER, "GatewaySMS", g_pDevCfg->cfgGatewaySMS, CONFIG_INI_FILE);
	n = ini_puts(SECTION_SERVER, "ReportSMS", g_pDevCfg->cfgReportSMS, CONFIG_INI_FILE);
	n = ini_puts(SECTION_SERVER, "GatewayIP", g_pDevCfg->cfgGatewayIP, CONFIG_INI_FILE);
	n = ini_puts(SECTION_SERVER, "Config_URL", g_pDevCfg->cfgConfig_URL, CONFIG_INI_FILE);
	n = ini_puts(SECTION_SERVER, "Upload_URL", g_pDevCfg->cfgUpload_URL, CONFIG_INI_FILE);

	n = ini_puts("SIM1", "APN", g_pDevCfg->SIM[0].cfgAPN, CONFIG_INI_FILE);
	n = ini_puts("SIM2", "APN", g_pDevCfg->SIM[1].cfgAPN, CONFIG_INI_FILE);

	intervals = &g_pDevCfg->sIntervalsMins;
	n = ini_putl(SECTION_INTERVALS, "Sampling", intervals->sampling, CONFIG_INI_FILE);
	if (n == 0)
	return;

	n = ini_putl(SECTION_INTERVALS, "Upload", intervals->upload, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "Reboot", intervals->systemReboot, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "Configuration", intervals->configurationFetch, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "SMS_Check", intervals->smsCheck, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "Network_Check", intervals->networkCheck, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "LCD_off", intervals->lcdOff, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "Alarms", intervals->alarmsCheck, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "ModemPullTime", intervals->modemPullTime, CONFIG_INI_FILE);
	n = ini_putl(SECTION_INTERVALS, "BatteryCheck", intervals->batteryCheck, CONFIG_INI_FILE);
}
#endif

FRESULT config_read_ini_file() {
	FRESULT fr;
	FILINFO fno;

	long n = 0;

	LOGGING_COMPONENTS *cfg;
	INTERVAL_PARAM *intervals;
	SYSTEM_SWITCHES *system; //pointer to system_switches struct

	//TODO: SYSTEM_SWITCHES for buzzer from ini file

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	fr = f_stat(CONFIG_INI_FILE, &fno);
	if (fr == FR_NO_FILE) {
		return fr;
	}

	log_append_("Read INI");

#ifdef _DEBUG
	n = ini_gets("SYSTEM", "Version", __DATE__, g_pDevCfg->cfgVersion,
			sizearray(g_pDevCfg->cfgVersion), CONFIG_INI_FILE);
	if (n == 0)
		return FR_NO_FILE;

#endif

	cfg = &g_pDevCfg->cfg;
	cfg->logs.system_log = ini_getbool(SECTION_LOGS, "SystemLog", 0,
	CONFIG_INI_FILE);
	cfg->logs.web_csv = ini_getbool(SECTION_LOGS, "WebCSV", 0, CONFIG_INI_FILE);
	cfg->logs.server_config = ini_getbool(SECTION_LOGS, "ServerConfig", 0,
	CONFIG_INI_FILE);
	cfg->logs.modem_transactions = ini_getbool(SECTION_LOGS, "Modem", 0,
	CONFIG_INI_FILE);
	cfg->logs.sms_alerts = ini_getbool(SECTION_LOGS, "SMS_Alerts", 0,
	CONFIG_INI_FILE);
	cfg->logs.sms_reports = ini_getbool(SECTION_LOGS, "SMS_Reports", 0,
	CONFIG_INI_FILE);

	// Currently in [LOGS] section
	system = &g_pSysState->system; //switches initialized in system_states
	system->switches.buzzer_disabled = ini_getbool(SECTION_LOGS, "Buzzer_Off",
			0,
			CONFIG_INI_FILE);

	n = ini_gets(SECTION_SERVER, "GatewaySMS", NEXLEAF_SMS_GATEWAY,
			g_pDevCfg->cfgGatewaySMS, sizearray(g_pDevCfg->cfgGatewaySMS),
			CONFIG_INI_FILE);

	if (n == 0)
		return FR_NO_FILE;

	n = ini_gets(SECTION_SERVER, "ReportSMS", REPORT_PHONE_NUMBER,
			g_pDevCfg->cfgReportSMS, sizearray(g_pDevCfg->cfgReportSMS),
			CONFIG_INI_FILE);

	n = ini_gets(SECTION_SERVER, "GatewayIP", NEXLEAF_DEFAULT_SERVER_IP,
			g_pDevCfg->cfgGatewayIP, sizearray(g_pDevCfg->cfgGatewayIP),
			CONFIG_INI_FILE);

	n = ini_gets(SECTION_SERVER, "Config_URL", CONFIGURATION_URL_PATH,
			g_pDevCfg->cfgConfig_URL, sizearray(g_pDevCfg->cfgConfig_URL),
			CONFIG_INI_FILE);
	n = ini_gets(SECTION_SERVER, "Upload_URL", DATA_UPLOAD_URL_PATH,
			g_pDevCfg->cfgUpload_URL, sizearray(g_pDevCfg->cfgUpload_URL),
			CONFIG_INI_FILE);

	n = ini_gets("SIM1", "APN", NEXLEAF_DEFAULT_APN, g_pDevCfg->SIM[0].cfgAPN,
			sizearray(g_pDevCfg->cfgConfig_URL), CONFIG_INI_FILE);
	n = ini_gets("SIM2", "APN", NEXLEAF_DEFAULT_APN, g_pDevCfg->SIM[1].cfgAPN,
			sizearray(g_pDevCfg->cfgConfig_URL), CONFIG_INI_FILE);

	intervals = &g_pDevCfg->sIntervalsMins;
	intervals->sampling = ini_getl(SECTION_INTERVALS, "Sampling",
	PERIOD_SAMPLING, CONFIG_INI_FILE);
	intervals->upload = ini_getl(SECTION_INTERVALS, "Upload", PERIOD_UPLOAD,
	CONFIG_INI_FILE);
	;
	intervals->systemReboot = ini_getl(SECTION_INTERVALS, "Reboot",
	PERIOD_REBOOT, CONFIG_INI_FILE);
	intervals->configurationFetch = ini_getl(SECTION_INTERVALS, "Configuration",
	PERIOD_CONFIGURATION_FETCH, CONFIG_INI_FILE);
	intervals->smsCheck = ini_getl(SECTION_INTERVALS, "SMS_Check",
	PERIOD_SMS_CHECK, CONFIG_INI_FILE);
	intervals->networkCheck = ini_getl(SECTION_INTERVALS, "Network_Check",
	PERIOD_NETWORK_CHECK, CONFIG_INI_FILE);
	intervals->lcdOff = ini_getl(SECTION_INTERVALS, "LCD_off", PERIOD_LCD_OFF,
	CONFIG_INI_FILE);
	intervals->alarmsCheck = ini_getl(SECTION_INTERVALS, "Alarms",
	PERIOD_ALARMS_CHECK, CONFIG_INI_FILE);
	intervals->modemPullTime = ini_getl(SECTION_INTERVALS, "ModemPullTime",
	PERIOD_PULLTIME, CONFIG_INI_FILE);
	intervals->batteryCheck = ini_getl(SECTION_INTERVALS, "BatteryCheck",
	PERIOD_BATTERY_CHECK, CONFIG_INI_FILE);

	if (g_bServiceMode) {
		config_display_config();
	}

#ifndef _DEBUG
	//fr = f_rename(CONFIG_INI_FILE, "thermal.old");
#endif
	_NOP();

#ifdef CONFIG_SAVE_IN_PROGRESS
	config_save_ini();
#endif
	return fr;
}

#endif

int config_default_configuration() {
	int i = 0;
	TEMP_ALERT_PARAM *alert;
	BATT_POWER_ALERT_PARAM *power;
	for (i = 0; i < SYSTEM_NUM_SENSORS; i++) {
		alert = &g_pDevCfg->stTempAlertParams[i];

		alert->maxSecondsCold = MINUTES_(ALARM_LOW_TEMP_PERIOD);
		alert->maxSecondsHot = MINUTES_(ALARM_HIGH_TEMP_PERIOD);

		alert->threshCold = LOW_TEMP_THRESHOLD;
		alert->threshHot = HIGH_TEMP_THRESHOLD;
	}

	power = &g_pDevCfg->stBattPowerAlertParam;
	power->enablePowerAlert = POWER_ENABLE_ALERT;
	power->minutesBattThresh = ALARM_BATTERY_PERIOD;
	power->minutesPower = ALARM_POWER_PERIOD;
	power->battThreshold = BATTERY_HIBERNATE_THRESHOLD;

// TODO: default values for own number & sms center?

	g_pDevCfg->sIntervalsMins.sampling = PERIOD_SAMPLING;
	g_pDevCfg->sIntervalsMins.upload = PERIOD_UPLOAD;
	g_pDevCfg->sIntervalsMins.systemReboot = PERIOD_REBOOT;
	g_pDevCfg->sIntervalsMins.configurationFetch = PERIOD_CONFIGURATION_FETCH;
	g_pDevCfg->sIntervalsMins.smsCheck = PERIOD_SMS_CHECK;

	g_pDevCfg->cfg.logs.sms_alerts = ALERTS_SMS;

#ifdef _DEBUG
	g_pDevCfg->cfg.logs.commmands = 1;
#endif

// Battery and power alarms
	return 1;
}
