/*
 * stringutils.h
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_STRINGUTILS_H_
#define TEMPSENSOR_V1_STRINGUTILS_H_

extern char* itoa_withpadding(int num);
extern char* itoa_nopadding(int num);

extern int _outc(char c, void *_op);
extern int _outs(char *s, void *_op, int len);


#endif /* TEMPSENSOR_V1_STRINGUTILS_H_ */
