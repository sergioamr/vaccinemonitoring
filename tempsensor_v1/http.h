/*
 * http.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_HTTP_H_
#define TEMPSENSOR_V1_HTTP_H_

extern void dohttpsetup();
extern void deactivatehttp();
extern int doget();
extern int dopost(char* postdata);
extern int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize);


#endif /* TEMPSENSOR_V1_HTTP_H_ */
