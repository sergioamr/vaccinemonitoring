/*
 * util.c
 *
 *  Created on: Mar 16, 2015
 *      Author: rajeevnx
 */
#include "stdint.h"
#include "util.h"

char tmpstr[10];


char* itoa_nopadding(int num)
{
	uint8_t     digit = 0;
	uint8_t		iIdx = 0;
	uint8_t		iCnt = 0;
	char        str[10];

	memset(tmpstr,0,sizeof(tmpstr));

	if(num == 0)
	{
		tmpstr[0] = 0x30;
		//tmpstr[1] = 0x30;
	}
	else
	{
		while (num != 0)
		{
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num/10;
		}

		//pad with zero if single digit
		if(iIdx == 1)
		{
			//tmpstr[0] = 0x30; //leading 0
			tmpstr[0] = str[0];
		}
		else
		{
			while (iIdx)
			{
				iIdx = iIdx - 1;
				tmpstr[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return tmpstr;
}

char* itoa(int num)
{
	uint8_t     digit = 0;
	uint8_t		iIdx = 0;
	uint8_t		iCnt = 0;
	char        str[10];

	memset(tmpstr,0,sizeof(tmpstr));

	if(num == 0)
	{
		tmpstr[0] = 0x30;
		tmpstr[1] = 0x30;
	}
	else
	{
		while (num != 0)
		{
			digit = (num % 10) + 0x30;
			str[iIdx] = digit;
			iIdx++;
			num = num/10;
		}

		//pad with zero if single digit
		if(iIdx == 1)
		{
			tmpstr[0] = 0x30; //leading 0
			tmpstr[1] = str[0];
		}
		else
		{
			while (iIdx)
			{
				iIdx = iIdx - 1;
				tmpstr[iCnt] = str[iIdx];
				iCnt = iCnt + 1;
			}
		}
	}

	return tmpstr;
}
