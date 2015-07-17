#include "thermalcanyon.h"
#include "state_machine.h"

//extern FRESULT sync_fs( /* FR_OK: successful, FR_DISK_ERR: failed */
// FATFS* fs /* File system object */
//);

#pragma SET_DATA_SECTION(".helpers")
char g_szFatFileName[32];
#pragma SET_DATA_SECTION()

char g_bFatInitialized = false;
char g_bLogDisabled = false;

#pragma SET_DATA_SECTION(".aggregate_vars")
FATFS FatFs; /* Work area (file system object) for logical drive */
const char *g_szLastSD_CardError = NULL;
uint8_t g_fatFileCaptured = 0;
FILINFO g_fatFili;
DIR g_fatDir;
FIL g_fatFilr;
#pragma SET_DATA_SECTION()

//-------------------------------------------------------------------------------------------

DIR *fat_getDirectory() {
	return &g_fatDir;
}

FILINFO *fat_getInfo() {
	return &g_fatFili;
}

FIL *fat_getFile() {
	return &g_fatFilr;
}

FRESULT fat_open(FIL **fobj, char *path, BYTE mode) {
	FRESULT res;

	if (g_fatFileCaptured == 1) {
		_NOP();
	}

	res = f_open(&g_fatFilr, path, mode);
	*fobj = &g_fatFilr;

	if (res == FR_OK) {
		g_fatFileCaptured = 1;
	}
	return res;
}

FRESULT fat_close() {
	if (g_fatFileCaptured == 0) {
		_NOP();
		return FR_INT_ERR;
	}

	g_fatFileCaptured = 0;
	return f_close(&g_fatFilr);
}

//-------------------------------------------------------------------------------------------

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

char* get_YMD_String(struct tm* timeData) {
	static char g_szYMDString[16];

	g_szYMDString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szYMDString, "0000");
	else
		strcpy(g_szYMDString, itoa_nopadding(timeData->tm_year));

	strcat(g_szYMDString, itoa_pad(timeData->tm_mon));
	strcat(g_szYMDString, itoa_pad(timeData->tm_mday));
	return g_szYMDString;
}

char* get_date_string(struct tm* timeData, const char* dateSeperator,
		const char* dateTimeSeperator, const char* timeSeparator,
		uint8_t includeTZ) {
	static char g_szDateString[24]; // "YYYY-MM-DD HH:MM:SS IST"

	g_szDateString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szDateString, "0000");
	else
		strcpy(g_szDateString, itoa_nopadding(timeData->tm_year));

	strcat(g_szDateString, dateSeperator);
	strcat(g_szDateString, itoa_pad(timeData->tm_mon));
	strcat(g_szDateString, dateSeperator);
	strcat(g_szDateString, itoa_pad(timeData->tm_mday));
	strcat(g_szDateString, dateTimeSeperator);
	strcat(g_szDateString, itoa_pad(timeData->tm_hour));
	strcat(g_szDateString, timeSeparator);
	strcat(g_szDateString, itoa_pad(timeData->tm_min));
	strcat(g_szDateString, timeSeparator);
	strcat(g_szDateString, itoa_pad(timeData->tm_sec));

	//[TODO] Check daylight saving time it doesnt work?

	/*
	 if (includeTZ && timeData->tm_isdst) {
	 strcat(g_szDateString, " DST");
	 }
	 */
	return g_szDateString;
}

// FORMAT IN FORMAT [YYYYMMDD:HHMMSS] Used for SMS timestamp
char* get_simplified_date_string(struct tm* timeData) {
	static char g_szDateString[26]; // "YYYY-MM-DD HH:MM:S IST"

	if (timeData == NULL)
		timeData = &g_tmCurrTime;

	g_szDateString[0] = 0;
	if (timeData->tm_year < 1900 || timeData->tm_year > 3000) // Working for 1000 years?
		strcpy(g_szDateString, "0000");
	else
		strcpy(g_szDateString, itoa_nopadding(timeData->tm_year));

	strcat(g_szDateString, itoa_pad(timeData->tm_mon));
	strcat(g_szDateString, itoa_pad(timeData->tm_mday));
	strcat(g_szDateString, ":");
	strcat(g_szDateString, itoa_pad(timeData->tm_hour));
	strcat(g_szDateString, itoa_pad(timeData->tm_min));
	strcat(g_szDateString, itoa_pad(timeData->tm_sec));
	return g_szDateString;
}

void parse_time_from_line(struct tm* timeToConstruct, char* formattedLine) {
	char dateAttribute[5];
	char* token = NULL;

	token = strtok(formattedLine, ",");

	if (token != NULL) {
		strncpy(dateAttribute, &token[4], 4);
		dateAttribute[4] = 0;
		timeToConstruct->tm_year = atoi(&dateAttribute[0]);

		strncpy(dateAttribute, &token[8], 2);
		dateAttribute[2] = 0;
		timeToConstruct->tm_mon = atoi(&dateAttribute[0]);

		strncpy(dateAttribute, &token[10], 2);
		dateAttribute[2] = 0;
		timeToConstruct->tm_mday = atoi(&dateAttribute[0]);

		strncpy(dateAttribute, &token[12], 2);
		dateAttribute[2] = 0;
		timeToConstruct->tm_hour = atoi(&dateAttribute[0]);

		strncpy(dateAttribute, &token[14], 2);
		dateAttribute[2] = 0;
		timeToConstruct->tm_min = atoi(&dateAttribute[0]);

		strncpy(dateAttribute, &token[16], 2);
		dateAttribute[2] = 0;
		timeToConstruct->tm_sec = atoi(&dateAttribute[0]);
	}
}

void offset_timestamp(struct tm* dateToOffset, int intervalMultiplier) {
	int timeVal;

	if (dateToOffset == NULL)
		return;

	timeVal = dateToOffset->tm_min
			+ (intervalMultiplier * g_pDevCfg->sIntervalsMins.sampling);
	if (timeVal >= 60) {
		while (timeVal >= 60) {
			timeVal -= 60;
			dateToOffset->tm_min = timeVal;
			dateToOffset->tm_hour += 1;
		}
	} else {
		dateToOffset->tm_min = timeVal;
	}
}

// Takes a string with the same format that is stored in file $TS=TIMESTAMP,INTERVAL,
int date_within_interval(struct tm* timeToCompare, struct tm* baseTime,
		int interval) {
	struct tm baseInterval;
	int timeVal;

	if (timeToCompare == NULL || baseTime == NULL)
		return 0;

	// Generically this needs to work for 31/12/xxxx 23:59:59 + 1 second
	// (which is 01/01/(xxxx+1) 00:00:00 - In our case a different day = different file
	// so it only needs to be accurate up to hours.
	// Leniancy ~+-1 minute (not guaranteed to be 60 seconds but min = 60 and max = 119)
	memcpy(&baseInterval, baseTime, sizeof(struct tm));
	timeVal = baseTime->tm_min + interval;
	if (timeVal >= 60) {
		while (timeVal >= 60) {
			timeVal -= 60;
			baseInterval.tm_min = timeVal;
			baseInterval.tm_hour = baseTime->tm_hour + 1;
		}
	} else {
		baseInterval.tm_min = timeVal;
	}

	if (timeToCompare->tm_hour != baseInterval.tm_hour) {
		return 0;
	}

	if ((timeToCompare->tm_min <= baseInterval.tm_min + 1)
			&& (timeToCompare->tm_min >= baseInterval.tm_min - 1)) {
		return 1;
	}

	return 0;
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

	g_szLastSD_CardError = FR_ERRORS[fr];
	state_SD_card_problem(fr, FR_ERRORS[fr]);

	event_LCD_turn_on();
	lcd_printl(LINEC, "SD CARD FAILURE");
	lcd_printf(LINEH, "%s", FR_ERRORS[fr]);
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

	g_szLastSD_CardError = NULL;
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
	state_SD_card_OK();

	// Delete old config files
	f_unlink(LOG_MODEM_PATH);
	f_unlink(CONFIG_LOG_FILE_PATH);

	fr = log_append_(" ");
	fr = log_appendf("Boot %d", (int) g_pSysCfg->numberConfigurationRuns);
	fr = log_appendf("Last[%d]", g_pSysCfg->lastCommand);

	return fr;
}

FRESULT fat_save_config(char *text) {
	FIL *fobj;
	FRESULT fr;
	size_t len;
	int bw = 0;

	if (!g_pDevCfg->cfg.logs.server_config)
		return FR_OK;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	if (text == NULL)
		return FR_OK;

	len = strlen(text);
	if (len == 0)
		return FR_OK;

	fr = fat_open(&fobj, CONFIG_LOG_FILE_PATH, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if (fr != FR_OK) {
		fat_check_error(fr);
		return fr;
	}

	if (fobj->fsize) {
		//append to the file
		f_lseek(fobj, fobj->fsize);
	}

	fr = f_write(fobj, text, len, (UINT *) &bw);
	if (fr != FR_OK || bw != len) {
		lcd_print("Failed writing SD");
		fat_close();
		return fr;
	}

	fr = f_write(fobj, text, len, (UINT *) &bw);
	return fat_close();
}

FRESULT log_append_(char *text) {
	FIL *fobj;
	int t = 0;
	int len = 0;
	int bw = 0;
	char szLog[32];
	FRESULT fr;

	if (!g_pDevCfg->cfg.logs.system_log)
		return FR_OK;

	if (g_bLogDisabled)
		return FR_NOT_READY;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	if (text == NULL)
		return FR_OK;

	fr = fat_open(&fobj, LOG_FILE_PATH, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);

	if (fr == FR_OK) {
		if (fobj->fsize) {
			//append to the file
			f_lseek(fobj, fobj->fsize);
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
	fr = f_write(fobj, szLog, len, (UINT *) &bw);
	if (fr != FR_OK || bw != len) {
		lcd_print("Failed writing SD");
		fat_close();
		return fr;
	}

	len = strlen(text);
	if (len > 0) {
		for (t = 0; t < len; t++) {
			if (text[t] == '\n' || text[t] == '\r')
				text[t] = ' ';
		}

		fr = f_write(fobj, text, len, (UINT *) &bw);
	}
	strcpy(szLog, "\r\n");
	fr = f_write(fobj, szLog, strlen(szLog), (UINT *) &bw);
	return fat_close();
}

FRESULT log_modem(const char *text) {
	FIL *fobj;
	int len = 0;
	int bw = 0;
	FRESULT fr;

	if (g_bLogDisabled || !g_bFatInitialized)
		return FR_NOT_READY;

	len = strlen(text);
	if (len == 0)
		return FR_NOT_READY;

	fr = fat_open(&fobj, LOG_MODEM_PATH, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);

	if (fr == FR_OK) {
		if (fobj->fsize)
			f_lseek(fobj, fobj->fsize);
	} else
		return fr;

	fr = f_write(fobj, text, len, (UINT *) &bw);
	return fat_close();
}

void log_disable() {
	g_bLogDisabled = true;
}

void log_enable() {
	g_bLogDisabled = true;
}

// This function is called with the stack really full. We get this array from the stack for it to not create strange behaviour
FRESULT log_appendf(const char *_format, ...) {
	va_list _ap;
	char szTemp[40];

#ifdef _DEBUG
	checkStack();
#endif
	va_start(_ap, _format);
	vsnprintf(szTemp, sizeof(szTemp), _format, _ap);
	va_end(_ap);
	if (g_bLogDisabled)
		return FR_NOT_READY;

	return log_append_(szTemp);
}

#ifndef _DEBUG
const char HEADER_CSV[] = "\"Date\",\"Batt\",\"Power\","
		"\"Sensor A\",\"Sensor B\",\"Sensor C\",\"Sensor D\",\"Sensor E\","
		"\"Signal\",\"Net\"\r\n";
#else
const char HEADER_CSV[] = "D,B,P,A,B,C,D,E,S,N\r\n";
#endif

FRESULT log_write_header(FIL *fobj, UINT *pBw) {
	return f_write(fobj, HEADER_CSV, sizeof(HEADER_CSV) - 1, pBw);
}

const char * const POWER_STATE[] = { "Power Outage", "Power Available" };

// 0 Power outage
//  Power available
const char *getPowerStateString() {
	return POWER_STATE[!(P4IN & BIT4)];
}

FRESULT log_write_temperature(FIL *fobj, UINT *pBw) {
	char *szLog;
	UINT bw = 0;
	char *date;
	FRESULT fr;
	int8_t iBatteryLevel;
	int8_t iSignalLevel;
	char *network_state;

	date = get_date_string(&g_tmCurrTime, "-", " ", ":", 1);
	fr = f_write(fobj, date, strlen(date), &bw);
	if (fr != FR_OK)
		return fr;

	*pBw += bw;

	iBatteryLevel = batt_getlevel();
	iSignalLevel = state_getSignalPercentage();
	network_state = state_getNetworkState();

	szLog = getStringBufferHelper(NULL);
	sprintf(szLog, ",\"%d%%\",\"%s\",%s,%s,%s,%s,%s,%d,%s\r\n",
			(int) iBatteryLevel, getPowerStateString(),
			temperature_getString(0), temperature_getString(1),
			temperature_getString(2), temperature_getString(3),
			temperature_getString(4), (int) iSignalLevel, network_state);
	fr = f_write(fobj, szLog, strlen(szLog), &bw);
	releaseStringBufferHelper();
	return fr;
}

FRESULT log_sample_web_format(UINT *tbw) {
	FIL *fobj;
	UINT bw = 0;	//bytes written
	FRESULT fr;

	if (!g_pDevCfg->cfg.logs.web_csv)
		return FR_OK;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

#ifdef _DEBUG
	lcd_print("Saving sample");
#else
	lcd_print_progress();
#endif

	rtc_getlocal(&g_tmCurrTime);
	char* fn = get_current_fileName(&g_tmCurrTime, FOLDER_DATA, EXTENSION_DATA);

	fr = fat_open(&fobj, fn, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		if (fobj->fsize) {
			//append to the file
			f_lseek(fobj, fobj->fsize);
		} else {
			fr = log_write_header(fobj, &bw);
			if (fr != FR_OK) {
				fat_check_error(fr);
				fat_close();
				return fr;
			}
		}
	} else {
		fat_check_error(fr);
		return fr;
	}

	fr = log_write_temperature(fobj, &bw);
	*tbw = bw;

	if (fr == FR_OK) {
#ifdef _DEBUG
		lcd_printf(LINE2, "OK %d", *tbw);
#else
		event_force_event_by_id(EVT_DISPLAY, 0);
#endif
	} else {
		fat_check_error(fr);
	}
	return fat_close();
}

#pragma SET_DATA_SECTION(".helper_vars")
struct tm g_lastSampleTime;
#pragma SET_DATA_SECTION()

FRESULT log_sample_to_disk(UINT *tbw) {
	FIL *fobj;
	struct tm tempDate;
	char *szLog = NULL;
	int iBatteryLevel;
	char* fn;
	FRESULT fr = FR_OK;
	EVENT *evt;
	uint16_t iSamplePeriod = 0;

	if (!g_bFatInitialized)
		return FR_NOT_READY;

	int bw = 0;	//bytes written
	fn = get_current_fileName(&g_tmCurrTime, FOLDER_TEXT, EXTENSION_TEXT);

	evt = events_find(EVT_SAVE_SAMPLE_TEMP);
	iSamplePeriod = event_getIntervalMinutes(evt);

	fr = fat_open(&fobj, fn, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		if (fobj->fsize) {
			//append to the file
			f_lseek(fobj, fobj->fsize);
		}
	} else {
		fat_check_error(fr);
		return fr;
	}

	rtc_get(&g_tmCurrTime, &tempDate);

	// If current time is out of previous interval, log a new time stamp
	// to avoid time offset issues
	if (!date_within_interval(&tempDate, &g_lastSampleTime, iSamplePeriod)) {
		g_pSysState->system.switches.timestamp_on = 1;
	}

	memcpy(&g_lastSampleTime, &tempDate, sizeof(struct tm));

	// Log time stamp when it's a new day or when
	// time gets pulled.
	if (g_pSysState->system.switches.timestamp_on) {
		szLog = getStringBufferHelper(NULL);
		strcat(szLog, "$TS=");
		strcat(szLog, itoa_pad((tempDate.tm_year + 1900)));
		strcat(szLog, itoa_pad(tempDate.tm_mon));
		strcat(szLog, itoa_pad(tempDate.tm_mday));
		strcat(szLog, itoa_pad(tempDate.tm_hour));
		strcat(szLog, itoa_pad(tempDate.tm_min));
		strcat(szLog, itoa_pad(tempDate.tm_sec));
		strcat(szLog, ",");
		strcat(szLog, "R");
		strcat(szLog, itoa_pad(iSamplePeriod));
		strcat(szLog, ",");
		strcat(szLog, "\n");
		fr = f_write(fobj, szLog, strlen(szLog), (UINT *) &bw);

		if (bw > 0) {
			*tbw += bw;
		}
		releaseStringBufferHelper();
		g_pSysState->system.switches.timestamp_on = 0;
	}

	//get battery level
#ifndef BATTERY_DISABLED
	iBatteryLevel = batt_getlevel();
#else
	iBatteryLevel = 0;
#endif

	//log sample period, battery level, power plugged, temperature values
	szLog = getStringBufferHelper(NULL);
	sprintf(szLog, "%s,%d,%s,%s,%s,%s,%s\n", itoa_pad(iBatteryLevel),
			!(P4IN & BIT4), temperature_getString(0), temperature_getString(1),
			temperature_getString(2), temperature_getString(3),
			temperature_getString(4));

	fr = f_write(fobj, szLog, strlen(szLog), (UINT *) &bw);
	releaseStringBufferHelper();

	if (bw > 0) {
		*tbw += bw;
	}

	return fat_close();
}
