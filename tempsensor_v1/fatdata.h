/*
 * fatdata.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_FATDATA_H_
#define TEMPSENSOR_V1_FATDATA_H_

extern FATFS FatFs;
extern char* getYMDString(struct tm* timeData);
extern char* getCurrentFileName(struct tm* timeData);
extern FRESULT fat_initDrive();
extern FRESULT log_sample_to_disk(int* tbw);
extern FRESULT log_append_text(char *text);
extern FRESULT log_append(const char *_format, ...);

#define FOLDER_LOG  "/log"
#define FOLDER_DATA "/data"

#define LOG_FILE_PATH FOLDER_LOG "/system.log"
#define LOG_FILE_UNKNOWN FOLDER_DATA "/unknown.csv"

#endif /* TEMPSENSOR_V1_FATDATA_H_ */
