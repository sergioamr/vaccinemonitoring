/*
 * config.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */
#define CONFIG_C_

//#define RUN_CALIBRATION

#include "thermalcanyon.h"
#include "calib/calibration.h"
#include "time.h"
#include "stringutils.h"
#include "hardware_buttons.h"
#include "time.h"
#include "events.h"

// Setup mode in which we are at the moment
// Triggered by the Switch 3 button
int8_t g_iSystemSetup = -1;
int g_iSamplePeriod = SAMPLE_PERIOD;
int g_iUploadPeriod = UPLOAD_PERIOD;

#define RUN_CALIBRATION

/************************** BEGIN CONFIGURATION MEMORY ****************************************/

#pragma SET_DATA_SECTION(".ConfigurationArea")
CONFIG_DEVICE g_ConfigDevice;	// configuration of the device, APN, gateways, etc.
CONFIG_SYSTEM g_ConfigSystem;   // is system initialized, number of runs, and overall stats
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
}

void config_setSIMError(SIM_CARD_CONFIG *sim, char errorToken, uint16_t errorID,
		const char *error) {
	if (error == NULL || sim == NULL)
		return;

	memset(sim->simLastError, 0, sizeof(sim->simLastError));
	strncpy(sim->simLastError, error, sizeof(sim->simLastError) - 1);
	sim->simErrorState = errorID;
	sim->simErrorToken = errorToken;

	event_LCD_turn_on();
}

uint8_t config_isSimOperational() {
	uint8_t slot = g_pDevCfg->cfgSIM_slot;
	return g_pDevCfg->SIM[slot].simOperational;
}

// Returns the current structure containing the info for the current SIM selected
uint16_t config_getSIMError(int slot) {
	return g_pDevCfg->SIM[slot].simErrorState;
}

// Returns the current structure containing the info for the current SIM selected
SIM_CARD_CONFIG *config_getSIM() {
	uint8_t slot = g_pDevCfg->cfgSIM_slot;
	return &g_pDevCfg->SIM[slot];
}

uint8_t config_getSelectedSIM() {
	return g_pDevCfg->cfgSIM_slot;
}

void config_SIM_not_operational() {
	uint8_t slot = g_pDevCfg->cfgSIM_slot;
	g_pDevCfg->SIM[slot].simOperational = 0;
}

void config_SIM_operational() {
	uint8_t slot = g_pDevCfg->cfgSIM_slot;
	g_pDevCfg->SIM[slot].simOperational = 1;
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

void config_incLastCmd() {
	g_pSysCfg->lastCommand++;
}

// Stores what was the last command run and what time
void config_setLastCommand(uint16_t lastCmd) {
	char cmd[32];
	g_pSysCfg->lastCommand = lastCmd;

	if (lastCmd == COMMAND_LCDINIT)
		return;

	sprintf(cmd, "Command [%d] ", lastCmd);
	config_save_command(cmd);
}

// Stores what was the last command run and what time
void config_save_command(char *str) {

	static int lastMin = 0;
	static int lastSec = 0;

	rtc_getlocal(&g_tmCurrTime);

	if (lastMin != g_tmCurrTime.tm_min && lastSec != g_tmCurrTime.tm_sec) {
		strcpy(g_pSysCfg->lastCommandTime, itoa_pad(g_tmCurrTime.tm_hour));
		strcat(g_pSysCfg->lastCommandTime, ":");
		strcpy(g_pSysCfg->lastCommandTime, itoa_pad(g_tmCurrTime.tm_min));
		strcat(g_pSysCfg->lastCommandTime, ":");
		strcat(g_pSysCfg->lastCommandTime, itoa_pad(g_tmCurrTime.tm_sec));
	}

	log_append_(str);
}

void config_reconfigure() {
	g_pSysCfg->memoryInitialized = 0xFF;
	PMM_trigBOR();
	while (1)
		;
}

void config_init() {

	memset(&g_sEvents, 0, sizeof(g_sEvents));

	if (!check_address_empty(g_pSysCfg->memoryInitialized)) {

		// Check if the user is pressing the service mode
		// Service Button was pressed during bootup. Rerun calibration
		if (switch_check_service_pressed()) {
			lcd_printf(LINEC, "Service Mode");
			lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
			delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
			g_pSysCfg->calibrationFinished = 0;
		}

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

	strcpy(g_pDevCfg->cfgGatewayIP, NEXLEAF_DEFAULT_SERVER_IP); // HTTP server nextleaf
	strcpy(g_pDevCfg->cfgGatewaySMS, NEXLEAF_SMS_GATEWAY); // Gateway to nextleaf

	config_setSIMError(&g_pDevCfg->SIM[0], '+', NO_ERROR, "***FIRST SIM***");
	config_setSIMError(&g_pDevCfg->SIM[1], '+', NO_ERROR, "**SECOND  SIM**");

	strcpy(g_pDevCfg->SIM[0].cfgAPN, NEXLEAF_DEFAULT_APN);
	strcpy(g_pDevCfg->SIM[1].cfgAPN, NEXLEAF_DEFAULT_APN);

	// TODO: default values for own number & sms center?
	g_pDevCfg->stIntervalParam.loggingInterval = SAMPLE_PERIOD;
	g_pDevCfg->stIntervalParam.uploadInterval = UPLOAD_PERIOD;

	// Init System internals

	// Setup internal system counters and checks
	memset(g_pSysCfg, 0, sizeof(CONFIG_SYSTEM));

	// First run
	g_pSysCfg->numberConfigurationRuns = 1;

	// Value to check to make sure the structure is still the same size;
	g_pSysCfg->configStructureSize = sizeof(CONFIG_SYSTEM);
	g_pSysCfg->memoryInitialized = 1;

	// Set the date and time of compilation as firmware version
	strcpy(g_pSysCfg->firmwareVersion, "v(" __DATE__ ")");

	g_pSysCfg->maxSamplebuffer = 0;
	g_pSysCfg->maxATResponse = 0;
	g_pSysCfg->maxRXBuffer = 0;
	g_pSysCfg->maxTXBuffer = 0;

	lcd_printf(LINEC, "CONFIG MODE");
	lcd_printl(LINEH, g_pSysCfg->firmwareVersion); // Show the firmware version
#ifndef _DEBUG
			delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
#endif

	// First initalization, calibration code.
	calibrate_device();
}

void config_update_intervals() {
	g_iUploadPeriod = g_pDevCfg->stIntervalParam.uploadInterval;
	g_iSamplePeriod = g_pDevCfg->stIntervalParam.loggingInterval;
}

void config_update_system_time() {

	// Gets the current time and stores it in FRAM
	rtc_getlocal(&g_tmCurrTime);
	memcpy(&g_pDevCfg->lastSystemTime, &g_tmCurrTime, sizeof(g_tmCurrTime));
}

// http://54.241.2.213/coldtrace/uploads/multi/v3/358072043112124/1/
// $ST2,7,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,600,1,600,15,$ST1,919243100142,both,airtelgprs.com,10,1,0,0,$EN

int config_parse_configuration(char *msg) {
	char *token;
	char *delimiter = ",";
	int tempValue = 0;
	int iCnt = 0;
	SIM_CARD_CONFIG *sim = config_getSIM();

	PARSE_FINDSTR_BUFFER_RET(token, msg, "$ST2,", UART_FAILED);

	// Return success if no configuration has changed
	PARSE_FIRSTVALUECOMPARE(token, g_pDevCfg->cfgSyncId, UART_SUCCESS,
			UART_FAILED);
	int i = 0;

	// Temperature configuration for each sensor
	while (i < MAX_NUM_SENSORS) {
		PARSE_NEXTVALUE(token, &g_pDevCfg->stTempAlertParams[i].maxTimeCold,
				delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDevCfg->stTempAlertParams[i].threshCold,
				delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDevCfg->stTempAlertParams[i].maxTimeHot,
				delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDevCfg->stTempAlertParams[i].threshHot,
				delimiter, UART_FAILED);
		i++;
	}

	// Battery config info.
	PARSE_NEXTVALUE(token, &g_pDevCfg->stBattPowerAlertParam.minutesPower,
			delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDevCfg->stBattPowerAlertParam.enablePowerAlert,
			delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDevCfg->stBattPowerAlertParam.minutesBattThresh,
			delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDevCfg->stBattPowerAlertParam.battThreshold,
			delimiter, UART_FAILED);

	// SIM info
	PARSE_SKIP(token, delimiter, UART_FAILED); // $ST1
	PARSE_NEXTSTRING(token, &g_pDevCfg->cfgGatewaySMS[0], strlen(token),
			delimiter, UART_FAILED); // GATEWAY NUM
	PARSE_NEXTVALUE(token, &sim->cfgUploadMode, delimiter, UART_FAILED); // NETWORK TYPE E.G. GPRS
	PARSE_NEXTSTRING(token, &sim->cfgAPN[0], strlen(token), delimiter,
			UART_FAILED); //APN

	PARSE_NEXTVALUE(token, &g_pDevCfg->stIntervalParam.uploadInterval,
			delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDevCfg->stIntervalParam.loggingInterval,
			delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &tempValue, delimiter, UART_FAILED); // Reset alert
	if (tempValue > 0) {
		g_iStatus |= RESET_ALERT;
		//set buzzer OFF
		g_iStatus &= ~BUZZER_ON;
		//reset alarm state and counters
		for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
			//reset the alarm
			TEMP_ALARM_CLR(iCnt);
			//initialize confirmation counter
			g_iAlarmCnfCnt[iCnt] = 0;
		}
	} else {
		g_iStatus &= ~RESET_ALERT;
	}

	PARSE_NEXTVALUE(token, &tempValue, delimiter, UART_FAILED); ////
	if (g_pDevCfg->cfgSIM_slot != tempValue) {
		modem_swap_SIM();
	}

	PARSE_SKIP(token, delimiter, UART_FAILED); // $EN

	config_update_intervals();

	log_append_("configuration success");

	g_pDevCfg->cfgServerConfigReceived = 1;

	event_setInterval_by_id(EVT_SAMPLE_TEMP,
			g_pDevCfg->stIntervalParam.loggingInterval);
	event_setInterval_by_id(EVT_UPLOAD_SAMPLES,
			g_pDevCfg->stIntervalParam.uploadInterval);

	return UART_SUCCESS;
}

int config_process_configuration() {
	char *token;

	log_append_("configuration processing");
	// FINDSTR uses RXBuffer - There is no need to initialize the data to parse.
	PARSE_FINDSTR_RET(token, HTTP_INCOMING_DATA, UART_FAILED);
	return config_parse_configuration((char *) &RXBuffer[RXHeadIdx]);
}
