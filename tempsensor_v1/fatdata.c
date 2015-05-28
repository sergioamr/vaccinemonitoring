#include "thermalcanyon.h"

#pragma SET_DATA_SECTION(".aggregate_vars")
char g_szFatFileName[64];
FATFS FatFs; /* Work area (file system object) for logical drive */
#pragma SET_DATA_SECTION()

DWORD get_fattime(void) {
	DWORD tmr;

	rtc_getlocal(&currTime);
	/* Pack date and time into a DWORD variable */
	tmr = (((DWORD) currTime.tm_year - 1980) << 25)
			| ((DWORD) currTime.tm_mon << 21) | ((DWORD) currTime.tm_mday << 16)
			| (WORD) (currTime.tm_hour << 11) | (WORD) (currTime.tm_min << 5)
			| (WORD) (currTime.tm_sec >> 1);
	return tmr;
}

char szYMDString[4+2+2+1];

char* getYMDString(struct tm* timeData) {

	szYMDString[0]=0;
	if (timeData->tm_year<1900 || timeData->tm_year>3000) // Working for 1000 years?
		strcpy(szYMDString,"0000");
	else
		strcpy(szYMDString,itoa_nopadding(timeData->tm_year));

	strcat(szYMDString,itoa_withpadding(timeData->tm_mon+1));
	strcat(szYMDString,itoa_withpadding(timeData->tm_mday));
	return szYMDString;
}

char* getCurrentFileName(struct tm* timeData) {
	if (timeData->tm_mday==0 && timeData->tm_mon && timeData->tm_year==0) {
		strcpy(g_szFatFileName, "/data/unknown.csv");
		return g_szFatFileName;
	}

	sprintf(g_szFatFileName, "/data/%s.csv",getYMDString(timeData));
	return g_szFatFileName;
}
