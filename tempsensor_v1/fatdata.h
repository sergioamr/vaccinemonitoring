/*
 * fatdata.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_FATDATA_H_
#define TEMPSENSOR_V1_FATDATA_H_

extern FATFS FatFs;

char* get_YMD_String(struct tm* timeData);
char* get_current_fileName(struct tm* timeData);
FRESULT fat_init_drive();
FRESULT log_sample_to_disk(int* tbw);
FRESULT log_append_(char *text);
FRESULT log_appendf(const char *_format, ...);
FRESULT log_sample_web_format(UINT* tbw);

#define FOLDER_LOG  "/log"
#define FOLDER_DATA "/data"

#define LOG_FILE_PATH FOLDER_LOG "/system.log"
#define LOG_FILE_UNKNOWN FOLDER_DATA "/unknown.csv"

#endif /* TEMPSENSOR_V1_FATDATA_H_ */
