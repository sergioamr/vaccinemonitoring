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

#endif /* TEMPSENSOR_V1_FATDATA_H_ */
