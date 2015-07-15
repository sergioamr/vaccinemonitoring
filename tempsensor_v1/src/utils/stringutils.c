/*
 * stringutils.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include "stdint.h"
#include "string.h"
#include "globals.h"
#include "sms.h"

extern void checkStack();

#pragma SET_DATA_SECTION(".aggregate_vars")
char g_iStringCaptured = 0;
char g_szStringTemp[80];
char g_smsMsg[MAX_SMS_SIZE_FULL];
char g_szEncodedLine[80];
char g_szItoa[16];
#pragma SET_DATA_SECTION()

// Always release this buffer so you are sure to not use it more than once!!
char *getStringBufferHelper(uint16_t *size) {
	if (size!=NULL)
		*size = sizeof(g_szStringTemp);

	if (g_iStringCaptured) {
		__no_operation();
	}
	g_iStringCaptured = 1;
#ifdef _DEBUG
	memset(g_szStringTemp, 0x10, sizeof(g_szStringTemp));
#endif
	g_szStringTemp[0] = 0;
	return g_szStringTemp;
}

void releaseStringBufferHelper() {
	if (!g_iStringCaptured) {
		__no_operation();
	}
	g_iStringCaptured = 0;
}

char *getEncodedLineHelper(uint16_t *size) {
	if (size!=NULL)
		*size = sizeof(g_szEncodedLine);

#ifdef _DEBUG
	memset(g_szEncodedLine, 0x12, sizeof(g_szEncodedLine));
#endif
	g_szEncodedLine[0] = 0;
	return g_szEncodedLine;
}

char *getSMSBufferHelper() {
#ifdef _DEBUG
	memset(g_smsMsg, 0x12, sizeof(g_smsMsg));
#endif
	g_smsMsg[0] = 0;
	return g_smsMsg;
}

char *getFloatNumber2Text(float number, char *ret) {
	int i = 0;
	int8_t count = 0;

	memset(ret, 0, 6);

	//Round to one digit after decimal point
	int32_t fixedPoint = (int32_t) (number * 10);

	if (fixedPoint < TEMP_CUTOFF) {
		for (i = 0; i < 4; i++)
			ret[i] = '-';
		return ret;
	}

	if (fixedPoint < 0) {
		count = 1;
		ret[0] = '-';
		fixedPoint = abs(fixedPoint);
	}

	for (i = 2; i >= 0; i--) {
		ret[i + count] = fixedPoint % 10 + 0x30;
		fixedPoint = fixedPoint / 10;
	}

	ret[3 + count] = ret[2 + count];
	ret[2 + count] = (char) '.';
	return ret;
}

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


char* itoa_pad(int num) {
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

char* replace_character(char* string, char charToFind, char charToReplace) {
	int i = 0;

	while (string[i] != '\0') {
		if (string[i] == charToFind)
			string[i] = charToReplace;
		i++;
	}

	return string;
}


