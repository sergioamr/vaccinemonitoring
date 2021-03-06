/*
 * fatdata.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_FATDATA_H_
#define TEMPSENSOR_V1_FATDATA_H_

extern FATFS FatFs;
extern char g_bLogDisabled;
extern char g_bFatInitialized;
extern const char *g_szLastSD_CardError;

char* get_YMD_String(struct tm* timeData);
char* get_current_fileName(struct tm* timeData, const char *folder, const char *ext);
char* get_date_string(struct tm* timeData, const char *dateSeperator,
		const char *dateTimeSeperator, const char *timeSeparator, uint8_t includeTZ);
char* get_simplified_date_string(struct tm* timeData);
void parse_time_from_line(struct tm* timeToConstruct, char* formattedLine);
void offset_timestamp(struct tm* dateToOffset, int intervalMultiplier);
int date_within_interval(struct tm* timeToCompare, struct tm* baseTime, int interval);
FRESULT fat_init_drive();
FRESULT fat_save_config(char *text);
FRESULT log_sample_to_disk(UINT* tbw);
FRESULT log_append_(char *text);
FRESULT log_appendf(const char *_format, ...);
FRESULT log_sample_web_format(UINT* tbw);

void log_disable();
void log_enable();

FRESULT log_modem(const char *text);

#define FOLDER_LOG  "/LOG"
#define FOLDER_SYS  ""

// Web format data
#define EXTENSION_DATA "CSV"
#define FOLDER_DATA "/DATA"

// Old data transfer used for parsing and streaming
#define EXTENSION_TEXT "TXT"
#define FOLDER_TEXT "/TXT"

#define LOG_FILE_PATH FOLDER_LOG "/system.log"
#define LOG_MODEM_PATH FOLDER_LOG "/modem.log"
#define CONFIG_INI_FILE FOLDER_SYS "/thermal.ini"
#define CONFIG_LOG_FILE_PATH FOLDER_LOG "/config.log"
#define LOG_FILE_UNKNOWN FOLDER_DATA "/unknown.csv"

#endif /* TEMPSENSOR_V1_FATDATA_H_ */
