#include "thermalcanyon.h"
#include "stringutils.h"
#include "events.h"
#include "alarms.h"

extern FRESULT sync_fs( /* FR_OK: successful, FR_DISK_ERR: failed */
FATFS* fs /* File system object */
);

#pragma SET_DATA_SECTION(".aggregate_vars")
char g_szFatFileName[64];
FATFS FatFs; /* Work area (file system object) for logical drive */
#pragma SET_DATA_SECTION()

char g_bFatInitialized = false;

const char * const FR_ERRORS[20] = { "OK", "DISK_ERR", "INT_ERR", "NOT_READY",
		"NO_FILE", "NO_PATH", "INVALID_NAME", "DENIED", "EXIST",
		"INVALID_OBJECT", "WRITE_PROTECTED", "INVALID_DRIVE", "NOT_ENABLED",
		"NO_FILESYSTEM", "MKFS_ABORTED", "TIMEOUT", "LOCKED", "NOT_ENOUGH_CORE",
		"TOO_MANY_OPEN_FILES", "INVALID_PARAMETER" };

DWORD get_fattime(void) {
	DWORD tmr;

	rtc_getlocal(&g_tmCurrTime);
	/* Pack date and time into a DWORD variable */
	tmr = (((DWORD) g_tmCurrTime.tm_year - 1980) << 25)
			| ((DWORD) g_tmCurrTime.tm_mon << 21)
			| ((DWORD) g_tmCurrTime.tm_mday << 16)
			| (WORD) (g_tmCurrTime.tm_hour << 11)
			| (WORD) (g_tmCurrTime.tm_min << 5)
			| (WORD) (g_tmCurrTime.tm_sec >> 1);
	return tmr;
}

char g_szYMDString[16];

char* get_YMD_String(struct tm* timeData) {

	g_szYMDString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szYMDString, "0000");
	else
		strcpy(g_szYMDString, itoa_nopadding(timeData->tm_year));

	strcat(g_szYMDString, itoa_pad(timeData->tm_mon + 1));
	strcat(g_szYMDString, itoa_pad(timeData->tm_mday));
	return g_szYMDString;
}

char g_szDateString[32]; // "YYYY-MM-DD HH:MM:S IST"

char* get_date_string(struct tm* timeData) {

	g_szDateString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szDateString, "0000");
	else
		strcpy(g_szDateString, itoa_nopadding(timeData->tm_year));

	strcat(g_szDateString, "-");
	strcat(g_szDateString, itoa_pad(timeData->tm_mon + 1));
	strcat(g_szDateString, "-");
	strcat(g_szDateString, itoa_pad(timeData->tm_mday));
	strcat(g_szDateString, " ");
	strcat(g_szDateString, itoa_pad(timeData->tm_hour));
	strcat(g_szDateString, ":");
	strcat(g_szDateString, itoa_pad(timeData->tm_min));
	strcat(g_szDateString, ":");
	strcat(g_szDateString, itoa_pad(timeData->tm_sec));

	//[TODO] Check timezone

	if (timeData->tm_isdst == 1) {
		strcat(g_szDateString, " DST");
	}
	return g_szDateString;
}

// FORMAT IN FORMAT [YYYYMMDD:HHMMSS] Used for SMS timestamp
char* get_simplified_date_string(struct tm* timeData) {

	g_szDateString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szDateString, "0000");
	else
		strcpy(g_szDateString, itoa_nopadding(timeData->tm_year));

	strcat(g_szDateString, itoa_pad(timeData->tm_mon + 1));
	strcat(g_szDateString, itoa_pad(timeData->tm_mday));
	strcat(g_szDateString, ":");
	strcat(g_szDateString, itoa_pad(timeData->tm_hour));
	strcat(g_szDateString, itoa_pad(timeData->tm_min));
	strcat(g_szDateString, itoa_pad(timeData->tm_sec));
	return g_szDateString;
}

char* get_current_fileName(struct tm* timeData, const char *folder,
		const char *ext) {
	if (timeData->tm_mday == 0 && timeData->tm_mon && timeData->tm_year == 0) {
		strcpy(g_szFatFileName, LOG_FILE_UNKNOWN);
		return g_szFatFileName;
	}

	sprintf(g_szFatFileName, "%s/%s.%s", folder, get_YMD_String(timeData), ext);
	return g_szFatFileName;
}

void fat_check_error(FRESULT fr) {
	if (fr == FR_OK)
		return;

	if (fr == FR_DISK_ERR || fr == FR_NOT_READY)
		g_bFatInitialized = false;

	alarm_sd_card_problem(fr);

	event_LCD_turn_on();
	lcd_printl(LINEC, "SD CARD FAILURE");
	lcd_printf(LINEE, "%s", FR_ERRORS[fr]);
}

FRESULT fat_create_folders() {
	FRESULT fr;

	fr = f_mkdir(FOLDER_LOG);
	if (fr != FR_EXIST)
		fat_check_error(fr);

	fr = f_mkdir(FOLDER_TEXT);
	if (fr != FR_EXIST)
		fat_check_error(fr);

	fr = f_mkdir(FOLDER_DATA);
	if (fr != FR_EXIST)
		fat_check_error(fr);

	// Remount fat, we have to check why is it not updating the structure in memory
	fr = f_mount(&FatFs, "", 0);
	fat_check_error(fr);
	if (fr != FR_OK)
		return fr;

	return fr;
}

FRESULT fat_init_drive() {
	FRESULT fr;
	FILINFO fno;

	/* Register work area to the default drive */
	fr = f_mount(&FatFs, "", 0);
	fat_check_error(fr);
	if (fr != FR_OK)
		return fr;

	fr = f_stat(FOLDER_DATA, &fno);
	if (fr == FR_NO_FILE) {
		fr = fat_create_folders();
	}

	fat_check_error(fr);
	if (fr != FR_OK)
		return fr;

	// Fat is ready
	g_bFatInitialized = true;

	fr = log_append_("");
	fr = log_append_("*****************************************************");
	fr = log_appendf("Start Boot %d", (int) g_pSysCfg->numberConfigurationRuns);
	return fr;
}

FRESULT log_append_(char *text) {
	FIL fobj;
	int t = 0;
	int len = 0;
	int bw = 0;
	char szLog[32];
	FRESULT fr;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	if (text == NULL)
		return FR_OK;

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

	rtc_getlocal(&g_tmCurrTime);

	strcpy(szLog, "[");
	if (g_tmCurrTime.tm_year > 2000) {
		strcat(szLog, get_YMD_String(&g_tmCurrTime));
		strcat(szLog, " ");
		strcat(szLog, itoa_pad(g_tmCurrTime.tm_hour));
		strcat(szLog, itoa_pad(g_tmCurrTime.tm_min));
		strcat(szLog, itoa_pad(g_tmCurrTime.tm_sec));
	} else {
		for (t = 0; t < 15; t++)
			strcat(szLog, "*");
	}
	strcat(szLog, "] ");

	len = strlen(szLog);
	fr = f_write(&fobj, szLog, len, (UINT *) &bw);
	if (fr != FR_OK || bw != len) {
		lcd_print("Failed writing SD");
		f_close(&fobj);
		return fr;
	}

	len = strlen(text);
	if (len > 0) {
		for (t = 0; t < len; t++) {
			if (text[t] == '\n' || text[t] == '\r')
				text[t] = ' ';
		}

		fr = f_write(&fobj, text, len, (UINT *) &bw);
	}
	strcpy(szLog, "\r\n");
	fr = f_write(&fobj, szLog, strlen(szLog), (UINT *) &bw);

	f_sync(&fobj);
	return f_close(&fobj);
}

FRESULT log_appendf(const char *_format, ...) {
	char szTemp[180];

	va_list _ap;
	char *fptr = (char *) _format;
	char *out_end = szTemp;

	va_start(_ap, _format);
	__TI_printfi(&fptr, _ap, (void *) &out_end, _outc, _outs);
	va_end(_ap);

	*out_end = '\0';
	return log_append_(szTemp);
}

const char HEADER_CSV[] =
		"\"Date of Reading\",\"Battery %\",\"Power Status\",\"Sensor A (Deg. C)\",\"Sensor B (Deg. C)\",\"Sensor C (Deg. C)\",\"Sensor D (Deg. C)\",\"Sensor E (Deg. C)\"\r\n";

FRESULT log_write_header(FIL *fobj, UINT *pBw) {
	return f_write(fobj, HEADER_CSV, sizeof(HEADER_CSV) - 1, pBw);
}

const char * const POWER_STATE[] = { "Power Outage", "Power Available" };

// 0 Power outage
//  Power available
char getPowerState() {
	return !(P4IN & BIT4);
}

const char *getPowerStateString() {
	char pos = getPowerState();
	return POWER_STATE[pos];
}

FRESULT log_write_temperature(FIL *fobj, UINT *pBw) {
	char szLog[64];
	UINT bw = 0;
	char *date;
	FRESULT fr;

	date = get_date_string(&g_tmCurrTime);
	fr = f_write(fobj, date, strlen(date), &bw);
	if (fr != FR_OK)
		return fr;

	*pBw += bw;

	g_iBatteryLevel = batt_getlevel();

	sprintf(szLog, ",\"%d%%\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\r\n",
			g_iBatteryLevel, getPowerStateString(), Temperature[0],
			Temperature[1], Temperature[2], Temperature[3], Temperature[4]);
	fr = f_write(fobj, szLog, strlen(szLog), &bw);
	return fr;
}

FRESULT log_sample_web_format(UINT *tbw) {
	FIL fobj;
	UINT bw = 0;	//bytes written
	FRESULT fr;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	lcd_print("Saving sample");

	rtc_getlocal(&g_tmCurrTime);
	char* fn = get_current_fileName(&g_tmCurrTime, FOLDER_DATA, EXTENSION_DATA);

	fr = f_open(&fobj, fn, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		if (fobj.fsize) {
			//append to the file
			f_lseek(&fobj, fobj.fsize);
		} else {
			fr = log_write_header(&fobj, &bw);
			if (fr != FR_OK) {
				fat_check_error(fr);
				f_close(&fobj);
				return fr;
			}
		}
	} else {
		fat_check_error(fr);
		return fr;
	}

	fr = log_write_temperature(&fobj, &bw);
	*tbw = bw;

	if (fr == FR_OK) {
		f_sync(&fobj);
		lcd_printf(LINE2, "OK %d bytes", *tbw);
		f_sync(&fobj);
	} else {
		fat_check_error(fr);
	}
	return f_close(&fobj);
}

FRESULT log_sample_to_disk(int* tbw) {
	FIL fobj;
	char szLog[64];
	FRESULT fr = FR_OK;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	int bw = 0;	//bytes written
	char* fn = get_current_fileName(&g_tmCurrTime, FOLDER_TEXT, EXTENSION_TEXT);

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
			rtc_get(&g_tmCurrTime);

#if 1
			memset(szLog, 0, sizeof(szLog));
			strcat(szLog, "$TS=");
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_year));
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_mon));
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_mday));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_hour));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_min));
			strcat(szLog, ":");
			strcat(szLog, itoa_pad(g_tmCurrTime.tm_sec));
			strcat(szLog, ",");
			strcat(szLog, "R");	//removed =
			strcat(szLog, itoa_pad(g_iSamplePeriod));
			strcat(szLog, ",");
			strcat(szLog, "\n");
			fr = f_write(&fobj, szLog, strlen(szLog), (UINT *) &bw);
#else
			bw = f_printf(fobj,"$TS=%04d%02d%02d:%02d:%02d:%02d,R=%d,", g_tmCurrTime.tm_year, g_tmCurrTime.tm_mon, g_tmCurrTime.tm_mday,
					g_tmCurrTime.tm_hour, g_tmCurrTime.tm_min, g_tmCurrTime.tm_sec,g_iSamplePeriod);
#endif
			if (bw > 0) {
				*tbw += bw;
				g_iStatus &= ~LOG_TIME_STAMP;
			}
		}

		//get battery level
#ifndef BATTERY_DISABLED
		g_iBatteryLevel = batt_getlevel();
#else
		g_iBatteryLevel = 0;
#endif
		//log sample period, battery level, power plugged, temperature values
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
		//bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,E=%s,", iBatteryLevel,
		//			  !(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3],Temperature[4]);
		memset(szLog, 0, sizeof(szLog));
		sprintf(szLog, "F%s,P%d,A%s,B%s,C%s,E%s,F%s\n",
				itoa_pad(g_iBatteryLevel), !(P4IN & BIT4), Temperature[0],
				Temperature[1], Temperature[2], Temperature[3], Temperature[4]);
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
		bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,", g_iBatteryLevel,
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
