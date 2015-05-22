#include "thermalcanyon.h"

FATFS FatFs; /* Work area (file system object) for logical drive */

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

char* getDMYString(struct tm* timeData) {
	char* dayMonthYear = "";
	strcat(dayMonthYear, itoa(timeData->tm_mday));
	strcat(dayMonthYear, itoa(timeData->tm_mon));
	strcat(dayMonthYear, itoa(timeData->tm_year));
	return dayMonthYear;
}

char* getCurrentFileName(struct tm* timeData) {
	char* fn = "/data/";
	strcat(fn, getDMYString(timeData));
	if(strcmp("/data/", fn) != 0 || strcmp("/data/000000", fn) == 0) {
		strcat(fn, "unknown");
	}
	strcat(fn, ".csv");
	return fn;
}
