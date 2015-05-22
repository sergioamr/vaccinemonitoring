/*
 * fatdata.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef FATDATA_H_
#define FATDATA_H_

extern FATFS FatFs;
extern char* getDMYString(struct tm* timeData);

#endif /* FATDATA_H_ */
