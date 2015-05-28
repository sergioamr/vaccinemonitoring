
/*
 * config.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */
#define CONFIG_C_


#include "thermalcanyon.h"
#include "calib/calibration.h"

int g_iSamplePeriod = SAMPLE_PERIOD;
int g_iUploadPeriod = UPLOAD_PERIOD;

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
	main_calibration();
#endif
	g_pSysCfg->calibrationFinished = 1;
	//reset the board by issuing a SW BOR
#ifdef RUN_CALIBRATION
	PMM_trigBOR();
	while(1);
#endif
}

void config_SafeMode() {
	_NOP();
}

void config_Init() {

	if (g_pSysCfg->memoryInitialized != 0xFF
			&& g_pSysCfg->memoryInitialized != 0x00) {

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

	// Setup internal system counters and checks
	memset(g_pSysCfg, 0, sizeof(CONFIG_SYSTEM));
	memset(g_pInfoA, 0, sizeof(CONFIG_INFOA));

	// Setup InfoA config data
	g_pInfoA->cfgSIMSlot=0;

	// First run
	g_pSysCfg->numberConfigurationRuns = 1;

	// Value to check to make sure the structure is still the same size;
	g_pSysCfg->configStructureSize = sizeof(CONFIG_SYSTEM);
	g_pSysCfg->memoryInitialized = 1;

	// Set the date and time of compilation as firmware version
	strcpy(g_pSysCfg->firmwareVersion, __DATE__ " " __TIME__);

	g_pSysCfg->maxSamplebuffer=0;
	g_pSysCfg->maxATResponse=0;
	g_pSysCfg->maxRXBuffer=0;
	g_pSysCfg->maxTXBuffer=0;

	// First initalization, calibration code.
	calibrate_device();
}
