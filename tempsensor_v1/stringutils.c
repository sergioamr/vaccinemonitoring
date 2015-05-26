/*
 * stringutils.c
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#include "stdint.h"
#include "string.h"
#include "globals.h"

char* itoa(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(g_szTemp, 0, sizeof(g_szTemp));

	if (num == 0) {
		g_szTemp[0] = 0x30;
		g_szTemp[1] = 0x30;
	} else {
		while (num != 0) {
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num / 10;
		}

		//pad with zero if single digit
		if (iIdx == 1) {
			g_szTemp[0] = 0x30; //leading 0
			g_szTemp[1] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				g_szTemp[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return g_szTemp;
}

char* itoa_nopadding(int num) {
	uint8_t digit = 0;
	uint8_t iIdx = 0;
	uint8_t iCnt = 0;
	char str[10];

	memset(g_szTemp, 0, sizeof(g_szTemp));

	if (num == 0) {
		g_szTemp[0] = 0x30;
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
			g_szTemp[0] = str[0];
		} else {
			while (iIdx) {
				iIdx = iIdx - 1;
				g_szTemp[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return g_szTemp;
}

