/*
 * config.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */
#define CONFIG_C_

#include "thermalcanyon.h"

int g_iSamplePeriod = SAMPLE_PERIOD;
int g_iUploadPeriod = UPLOAD_PERIOD;

#define __no_init    __attribute__ ((section (".noinit")))

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

void config_Init() {

	if (g_pSysCfg->memoryInitialized!=0xFF && g_pSysCfg->memoryInitialized!=0x00) {
		g_pSysCfg->numberConfigurationRuns++;
		return; // Memory was initialized, we are fine here.
	}

	g_pSysCfg->memoryInitialized=1;
	strcpy(g_pSysCfg->firmawareVersion, __DATE__ " " __TIME__ ); // Set the date and time of compilation as firmware version

	// First run
	g_pSysCfg->numberConfigurationRuns=0;

	// First initalization, calibration code.
	//calibration_run();
}
