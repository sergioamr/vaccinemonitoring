/*
 * stringutils.h
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_STRINGUTILS_H_
#define TEMPSENSOR_V1_STRINGUTILS_H_


#define zeroString(dest) dest[0]=0;
#define zeroTerminateCopy(dest, org) strncpy(dest, org, sizeof(dest)-1); dest[sizeof(dest)-1]=0;

char *getFloatNumber2Text(float number, char *ret);

// Everytime you get this buffer you have to release it!
char *getStringBufferHelper(uint16_t *size);
void releaseStringBufferHelper();

char *getEncodedLineHelper(uint16_t *size);
char *getSMSBufferHelper();

extern char* itoa_pad(int num);
extern char* itoa_nopadding(int num);
extern char* replace_character(char* string, char charToFind, char charToReplace);

extern int _outc(char c, void *_op);
extern int _outs(char *s, void *_op, int len);

extern _CODE_ACCESS int __TI_printfi(char **_format, va_list _ap, void *_op,
                                     int (*_outc)(char, void *),
                                     int (*_outs)(char *, void *,int));

#endif /* TEMPSENSOR_V1_STRINGUTILS_H_ */
