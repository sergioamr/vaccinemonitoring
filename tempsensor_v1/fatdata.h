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
extern void fat_initDrive();
extern FRESULT log_sample_to_disk(int* tbw);
extern FRESULT log_append(char *text);

#endif /* TEMPSENSOR_V1_FATDATA_H_ */
