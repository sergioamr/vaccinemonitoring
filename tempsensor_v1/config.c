
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

int g_iSamplePeriod = SAMPLE_PERIOD;
int g_iUploadPeriod = UPLOAD_PERIOD;

// Setup mode in which we are at the moment
// Triggered by the Switch 3 button
int8_t g_iSystemSetup = -1;

#define RUN_CALIBRATION

/************************** BEGIN CONFIGURATION MEMORY ****************************************/

#pragma SET_DATA_SECTION(".ConfigurationArea")
CONFIG_INFOA g_InfoA;
CONFIG_SYSTEM g_SystemConfig;
#pragma SET_DATA_SECTION()

#pragma SET_DATA_SECTION(".infoB")
CONFIG_INFOB g_InfoB;
#pragma SET_DATA_SECTION()

// Since we use the area outside the 16 bits we have to use the large memory model.
// The compiler gives a warning accessing the structure directly and it can fail. Better accessing it through the pointer.
CONFIG_INFOA* g_pInfoA = &g_InfoA;
CONFIG_INFOB* g_pInfoB = &g_InfoB;

CONFIG_SYSTEM* g_pSysCfg = &g_SystemConfig;
// Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.

/************************** END CONFIGURATION MEMORY ****************************************/

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
	sim->iErrorState= NO_ERROR;
}

void config_setSIMError(SIM_CARD_CONFIG *sim, uint16_t errorID, const char *error) {
	if (error == NULL || sim == NULL)
		return;
	strncpy(sim->simLastError, error, sizeof(sim->simLastError)-1);
	sim->iErrorState = errorID;
}

// Returns the current structure containing the info for the current SIM selected
uint16_t config_getSIMError(int slot) {
	return g_pInfoA->SIM[slot].iErrorState;
}

// Returns the current structure containing the info for the current SIM selected
SIM_CARD_CONFIG *config_getSIM() {

	uint8_t slot=g_pInfoA->cfgSIM_slot;
	return &g_pInfoA->SIM[slot];
}

uint8_t config_getSelectedSIM() {
	return g_pInfoA->cfgSIM_slot;
}

uint16_t config_getSimLastError() {
	return g_pInfoA->SIM[g_pInfoA->cfgSIM_slot].iErrorState;
}

void config_SafeMode() {
	_NOP();
}

void config_incLastCmd() {
	g_pSysCfg->lastCommand++;
}

// Stores what was the last command run and what time
void config_setLastCommand(uint16_t lastCmd)  {

	static int lastMin = 0;
	static int lastSec = 0;

	g_pSysCfg->lastCommand = lastCmd;

	rtc_get(&currTime);

	if (lastMin!=currTime.tm_min && lastSec!=currTime.tm_sec) {
		strcpy(g_pSysCfg->lastCommandTime,itoa_pad(currTime.tm_hour));
		strcat(g_pSysCfg->lastCommandTime,":");
		strcpy(g_pSysCfg->lastCommandTime,itoa_pad(currTime.tm_min));
		strcat(g_pSysCfg->lastCommandTime,":");
		strcat(g_pSysCfg->lastCommandTime,itoa_pad(currTime.tm_sec));
	}
}

void config_setup_extra_button() {
}

void config_init() {

	if (g_pSysCfg->memoryInitialized != 0xFF
			&& g_pSysCfg->memoryInitialized != 0x00) {

		// Check if the user is pressing the service mode
		// Service Button was pressed during bootup. Rerun calibration
		if (switch_check_service_pressed()) {
			lcd_clear();
			lcd_print_ext(LINE1, "Service Mode");
			lcd_print_lne(LINE2, g_pSysCfg->firmwareVersion); // Show the firmware version
			delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
			g_pSysCfg->calibrationFinished=0;
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
	memset(g_pInfoA, 0, sizeof(CONFIG_INFOA));

	// Setup InfoA config data
	g_pInfoA->cfgSIM_slot=0;

	strcpy(g_pInfoA->cfgGateway,SMS_NEXLEAF_GATEWAY); // Gateway to nextleaf

	config_setSIMError(&g_pInfoA->SIM[0], NO_ERROR, "FIRST SIM");
	config_setSIMError(&g_pInfoA->SIM[1], NO_ERROR, "SECOND SIM");

	// Init System internals

	// Setup internal system counters and checks
	memset(g_pSysCfg, 0, sizeof(CONFIG_SYSTEM));

	// First run
	g_pSysCfg->numberConfigurationRuns = 1;

	// Value to check to make sure the structure is still the same size;
	g_pSysCfg->configStructureSize = sizeof(CONFIG_SYSTEM);
	g_pSysCfg->memoryInitialized = 1;

	// Set the date and time of compilation as firmware version
	strcpy(g_pSysCfg->firmwareVersion, "v(" __DATE__  ")");

	g_pSysCfg->maxSamplebuffer=0;
	g_pSysCfg->maxATResponse=0;
	g_pSysCfg->maxRXBuffer=0;
	g_pSysCfg->maxTXBuffer=0;

	lcd_clear();
	lcd_print_ext(LINE1, "CONFIG MODE");
	lcd_print_lne(LINE2, g_pSysCfg->firmwareVersion); // Show the firmware version
#ifndef _DEBUG
	delay(HUMAN_DISPLAY_LONG_INFO_DELAY);
#endif

	// First initalization, calibration code.
	calibrate_device();
}
