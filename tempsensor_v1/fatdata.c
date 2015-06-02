#include "thermalcanyon.h"
#include "stringutils.h"

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

char szYMDString[4 + 2 + 2 + 1];

char* get_YMD_String(struct tm* timeData) {

	szYMDString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(szYMDString, "0000");
	else
		strcpy(szYMDString, itoa_nopadding(timeData->tm_year));

	strcat(szYMDString, itoa_pad(timeData->tm_mon + 1));
	strcat(szYMDString, itoa_pad(timeData->tm_mday));
	return szYMDString;
}

char* get_current_fileName(struct tm* timeData) {
	if (timeData->tm_mday == 0 && timeData->tm_mon && timeData->tm_year == 0) {
		strcpy(g_szFatFileName, LOG_FILE_UNKNOWN);
		return g_szFatFileName;
	}

	sprintf(g_szFatFileName, FOLDER_DATA "/%s.csv", get_YMD_String(timeData));
	return g_szFatFileName;
}

void fat_check_error(FRESULT fr) {
	if (fr == FR_OK)
		return;

	lcd_print_lne(LINE1, "SD CARD FAILURE");
	lcd_print_ext(LINE2, "ERROR 0x%x ", fr);
	delay(10000);
}

FRESULT fat_init_drive() {
	FRESULT fr;

	/* Register work area to the default drive */
	fr = f_mount(&FatFs, "", 0);
	fat_check_error(fr);
	if (fr != FR_OK)
		return fr;

	fr = f_mkdir(FOLDER_DATA);
	if (fr != FR_EXIST)
		fat_check_error(fr);

	fr = f_mkdir(FOLDER_LOG);
	if (fr != FR_EXIST)
		fat_check_error(fr);

	fr = log_append("Start Boot %d", (int) g_pSysCfg->numberConfigurationRuns);
	return fr;
}

FRESULT log_append_text(char *text) {

	FIL fobj;
	int bw = 0;	//bytes written
	int t = 0;

	fr = f_open(&fobj, LOG_FILE_PATH,
	FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		if (fobj.fsize) {
			//append to the file
			f_lseek(&fobj, fobj.fsize);
		}
	} else {
		fat_check_error(fr);
		return fr;
	}

	rtc_get(&currTime);

	strcpy(szLog, "[");
	if (currTime.tm_year > 2000) {
		strcat(szLog, get_YMD_String(&currTime));
		strcat(szLog, " ");
		strcat(szLog, itoa_pad(currTime.tm_hour));
		strcat(szLog, itoa_pad(currTime.tm_min));
		strcat(szLog, itoa_pad(currTime.tm_sec));
	} else {
		for (t = 0; t < 13; t++)
			strcat(szLog, "*");
	}
	strcat(szLog, "]");

	strcat(szLog, " ");
	strcat(szLog, text);
	strcat(szLog, "\r\n");

	fr = f_write(&fobj, szLog, strlen(szLog), (UINT *) &bw);

	f_sync(&fobj);
	return f_close(&fobj);
}

FRESULT log_append(const char *_format, ...) {
	char szTemp[180];

	va_list _ap;
	char *fptr = (char *) _format;
	char *out_end = szTemp;

	va_start(_ap, _format);
	__TI_printfi(&fptr, _ap, (void *) &out_end, _outc, _outs);
	va_end(_ap);

	*out_end = '\0';
	return log_append_text(szTemp);
}

FRESULT log_sample_to_disk(int* tbw) {
	FIL fobj;

	int bw = 0;	//bytes written
	char* fn = get_current_fileName(&currTime);

	if (!(g_iStatus & TEST_FLAG)) {
		fr = f_open(&fobj, fn, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
		if (fr == FR_OK) {
			if (fobj.fsize) {
				//append to the file
				f_lseek(&fobj, fobj.fsize);
			}
		} else {
			fat_check_error(fr);
			return fr;
		}

		if (g_iStatus & LOG_TIME_STAMP) {
			rtc_get(&currTime);

#if 1
			memset(szLog, 0, sizeof(szLog));
			strcat(szLog, "$TS=");
			strcat(szLog, itoa_pad(currTime.tm_year));
			strcat(szLog, itoa_pad(currTime.tm_mon));
			strcat(szLog, itoa_pad(currTime.tm_mday));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(currTime.tm_hour));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(currTime.tm_min));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(currTime.tm_sec));
			strcat(szLog, ",");
			strcat(szLog, "R");	//removed =
			strcat(szLog, itoa_pad(g_iSamplePeriod));
			strcat(szLog, ",");
			strcat(szLog, "\n");
			fr = f_write(&fobj, szLog, strlen(szLog), (UINT *) &bw);
#else
			bw = f_printf(fobj,"$TS=%04d%02d%02d:%02d:%02d:%02d,R=%d,", currTime.tm_year, currTime.tm_mon, currTime.tm_mday,
					currTime.tm_hour, currTime.tm_min, currTime.tm_sec,g_iSamplePeriod);
#endif
			if (bw > 0) {
				*tbw += bw;
				g_iStatus &= ~LOG_TIME_STAMP;
			}
		}

		//get battery level
#ifndef BATTERY_DISABLED
		iBatteryLevel = batt_getlevel();
#else
		iBatteryLevel = 0;
#endif
		//log sample period, battery level, power plugged, temperature values
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
		//bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,E=%s,", iBatteryLevel,
		//			  !(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3],Temperature[4]);
		memset(szLog, 0, sizeof(szLog));
		sprintf(szLog, "F%s,P%d,A%s,B%s,C%s,E%s,F%s\n", itoa_pad(iBatteryLevel),
				!(P4IN & BIT4), Temperature[0], Temperature[1], Temperature[2],
				Temperature[3], Temperature[4]);
		/*
		 strcat(szLog, "F");
		 strcat(szLog, itoa_pad(iBatteryLevel));
		 strcat(szLog, ",");
		 strcat(szLog, "P");
		 if (P4IN & BIT4) {
		 strcat(szLog, "0,");
		 } else {
		 strcat(szLog, "1,");
		 }
		 strcat(szLog, "A");
		 strcat(szLog, Temperature[0]);
		 strcat(szLog, ",B");
		 strcat(szLog, Temperature[1]);
		 strcat(szLog, ",C");
		 strcat(szLog, Temperature[2]);
		 strcat(szLog, ",D");
		 strcat(szLog, Temperature[3]);
		 strcat(szLog, ",E");
		 strcat(szLog, Temperature[4]);
		 strcat(szLog, ",");
		 strcat(szLog, "\n");
		 */

		fr = f_write(&fobj, szLog, strlen(szLog), (UINT *) &bw);
#else
		bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,", iBatteryLevel,
				!(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3]);
#endif

		if (bw > 0) {
			*tbw += bw;
		}

		f_sync(&fobj);
		return f_close(&fobj);
	} else {
		return FR_OK;
	}
}
