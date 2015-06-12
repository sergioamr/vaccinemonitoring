/*
 * rtc.c
 *
 *  Created on: Feb 5, 2015
 *      Author: rajeevnx
 */


#include "rtc.h"
#include "driverlib.h"
#include "stdlib.h"

Calendar g_rtcCalendarTime;

volatile uint32_t iMinuteTick=0;
const int8_t daysinMon[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

//local functions
void converttoUTC(struct tm* pTime);
void multiply16(int16_t op1, int16_t op2, uint32_t* pResult);
void multiply32(int16_t op1, int32_t op2, uint32_t* pResult);

void rtc_init(struct tm* pTime)
{
	if(pTime)
	{
		g_rtcCalendarTime.Seconds = pTime->tm_sec;
		g_rtcCalendarTime.Minutes = pTime->tm_min;
		g_rtcCalendarTime.Hours = pTime->tm_hour;
		g_rtcCalendarTime.DayOfWeek = pTime->tm_wday;
		g_rtcCalendarTime.DayOfMonth = pTime->tm_mday;
		g_rtcCalendarTime.Month = pTime->tm_mon;
		g_rtcCalendarTime.Year = pTime->tm_year;

	    //Initialize Calendar Mode of RTC
	    /*
	     * Base Address of the RTC_B
	     * Pass in current time, intialized above
	     * Use BCD as Calendar Register Format
	     */
	    RTC_B_initCalendar(RTC_B_BASE,
	                       &g_rtcCalendarTime,
	                       RTC_B_FORMAT_BINARY);

	    //Specify an interrupt to assert every minute
	    RTC_B_setCalendarEvent(RTC_B_BASE,
	                           RTC_B_CALENDAREVENT_MINUTECHANGE);

	    //Enable interrupt for RTC Ready Status, which asserts when the RTC
	    //Calendar registers are ready to read.
	    //Also, enable interrupts for the Calendar alarm and Calendar event.

	    RTC_B_clearInterrupt(RTC_B_BASE,
	                         RTCRDYIE + RTCTEVIE + RTCAIE);

	    //RTC_B_enableInterrupt(RTC_B_BASE,
	    //                      RTCRDYIE + RTCTEVIE + RTCAIE);

	    RTC_B_enableInterrupt(RTC_B_BASE,
	                          RTCTEVIE + RTCAIE);

	    //Start RTC Clock
	    RTC_B_startClock(RTC_B_BASE);

	}
}

void rtc_get(struct tm* pTime)
{
	g_rtcCalendarTime = RTC_B_getCalendarTime(RTC_B_BASE);

	if(pTime)
	{
		pTime->tm_sec = g_rtcCalendarTime.Seconds;
		pTime->tm_min = g_rtcCalendarTime.Minutes;
		pTime->tm_hour = g_rtcCalendarTime.Hours;
		pTime->tm_wday = g_rtcCalendarTime.DayOfWeek;
		pTime->tm_mon = g_rtcCalendarTime.Month;
		pTime->tm_mday = g_rtcCalendarTime.DayOfMonth;
		pTime->tm_year = g_rtcCalendarTime.Year;
		//convert to utc
		converttoUTC(pTime);
		//return mktime(pTime);
	}
}

void rtc_getlocal(struct tm* pTime)
{
	g_rtcCalendarTime = RTC_B_getCalendarTime(RTC_B_BASE);

	if(pTime)
	{
		pTime->tm_sec = g_rtcCalendarTime.Seconds;
		pTime->tm_min = g_rtcCalendarTime.Minutes;
		pTime->tm_hour = g_rtcCalendarTime.Hours;
		pTime->tm_wday = g_rtcCalendarTime.DayOfWeek;
		pTime->tm_mon = g_rtcCalendarTime.Month;
		pTime->tm_mday = g_rtcCalendarTime.DayOfMonth;
		pTime->tm_year = g_rtcCalendarTime.Year;
	}
}

#define ROLLBACK    	0x01
#define ADJUST_YEAR     0x02
#define ADJUST_MONTH    0x04

void converttoUTC(struct tm* pTime)
{
	uint32_t   timeinsecs = 0;
	uint32_t   tmpsecs = 0;
	uint32_t   tmpsecs32 = 0;
	uint8_t    adjustTime = 0;
	uint8_t    lastmday = 0;

	multiply16(pTime->tm_min, 60, &timeinsecs);
	multiply16(pTime->tm_hour, 3600, &tmpsecs);
	timeinsecs += tmpsecs;
	if(timeinsecs < 19800)
	{
		if(pTime->tm_mday > 1)
		{
			multiply32(pTime->tm_mday, 86400, &tmpsecs32);
		}
		else
		{
			//current month is even?
			//jan?
			if(pTime->tm_mon == 1)
			{
				lastmday = daysinMon[11];
				adjustTime |= ADJUST_YEAR | ADJUST_MONTH;
			}
			else
			{
				adjustTime |= ADJUST_MONTH;
				lastmday = daysinMon[pTime->tm_mon - 2];
				//check if current month is March
				if(pTime->tm_mon == 3)
				{
					//last month is Feb
					//leap year?
					if((pTime->tm_year % 4) == 0)
					{
						lastmday = 29;
					}
				}
			}
			multiply32(lastmday+1, 86400, &tmpsecs32);
		}
		timeinsecs += tmpsecs32;
		adjustTime |= ROLLBACK;
	}
	timeinsecs = timeinsecs - 19800;

	if(adjustTime & ROLLBACK)
	{
		pTime->tm_mday = timeinsecs/86400;
		multiply32(pTime->tm_mday, 86400, &tmpsecs32);
		timeinsecs -= tmpsecs32;
		if(adjustTime & ADJUST_YEAR)
		{
			pTime->tm_year--;
		}
		if(adjustTime & ADJUST_MONTH)
		{
			//jan?
			if(pTime->tm_mon == 1)
			{
				//last month is Dec
				pTime->tm_mon = 12;
			}
			else
			{
				pTime->tm_mon--;
			}
		}
	}

	pTime->tm_hour = timeinsecs/3600;
	multiply16(pTime->tm_hour, 3600, &tmpsecs);
	timeinsecs -= tmpsecs;
	pTime->tm_min = timeinsecs/60;

}

void multiply16(int16_t op1, int16_t op2, uint32_t* pResult)
{
  MPY32CTL0 &= ~OP2_32;
  MPYS = op1;
  OP2  = op2;
  *pResult = RESHI;
  *pResult = *pResult << 16;
  *pResult = *pResult | RESLO;
}

void multiply32(int16_t op1, int32_t op2, uint32_t* pResult)
{

  MPY32CTL0 |= OP2_32;
  MPYS = op1;
  OP2L = op2 & 0xFFFF;
  OP2H = op2 >> 16;
  *pResult = RESHI;
  *pResult = *pResult << 16;
  *pResult = *pResult | RESLO;

}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=RTC_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(RTC_VECTOR)))
#endif
void RTC_B_ISR(void)
{
    switch(__even_in_range(RTCIV,16))
    {
    case RTCIV_NONE: break;      //No interrupts
    case RTCIV_RTCRDYIFG:             //RTCRDYIFG
        //every second
        break;
    case RTCIV_RTCTEVIFG:             //RTCEVIFG
        //Interrupts every minute
        iMinuteTick++;

        break;
    case RTCIV_RTCAIFG:             //RTCAIFG
        //Interrupts 5:00pm on 5th day of week
        __no_operation();
        break;
    case RTCIV_RT0PSIFG: break;      //RT0PSIFG
    case RTCIV_RT1PSIFG: break;     //RT1PSIFG
    case RTCIV_RTCOFIFG: break;     //Reserved
    default: break;
    }
}
