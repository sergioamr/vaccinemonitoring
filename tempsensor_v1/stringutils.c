/*
 * stringutils.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include "stdint.h"
#include "string.h"
#include "globals.h"

char g_szItoa[16];

/*****************************************************************************/
/* _OUTC -  Put a character in a string                                      */
/*****************************************************************************/
int _outc(char c, void *_op)
{
    return *(*((char **)_op))++ = c;
}

/*****************************************************************************/
/* _OUTS -  Append a string to another string                                */
/*****************************************************************************/
int _outs(char *s, void *_op, int len)
{
    memcpy(*((char **)_op), s, len);
    *((char **)_op) += len;
    return len;
}


char* itoa_withpadding(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(g_szItoa, 0, sizeof(g_szItoa));

	if (num == 0) {
		g_szItoa[0] = 0x30;
		g_szItoa[1] = 0x30;
	} else {
		while (num != 0) {
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num / 10;
		}

		//pad with zero if single digit
		if (iIdx == 1) {
			g_szItoa[0] = 0x30; //leading 0
			g_szItoa[1] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				g_szItoa[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return g_szItoa;
}

char* itoa_nopadding(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(g_szItoa, 0, sizeof(g_szItoa));

	if (num == 0) {
		g_szItoa[0] = 0x30;
		//tmpstr[1] = 0x30;
	} else {
		while (num != 0) {
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num / 10;
		}

		//pad with zero if single digit
		if (iIdx == 1) {
			//tmpstr[0] = 0x30; //leading 0
			g_szItoa[0] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				g_szItoa[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return g_szItoa;
}


