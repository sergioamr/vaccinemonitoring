/*
 * rtc.h
 *
 *  Created on: Feb 5, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_RTC_H_
#define TEMPSENSOR_V1_RTC_H_


#include "time.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! \brief Initialize RTC.
//!
//! \param inittime is the initial calendar time.
//!
//! \return None
//
//*****************************************************************************
extern void rtc_init(struct tm* pTime);

//*****************************************************************************
//
//! \brief Get current RTC time in UTC format.
//!
//! \param stores the current time read from RTC.
//!
//! \return none
//
//*****************************************************************************
extern void  rtc_get(struct tm* pTime);

//*****************************************************************************
//
//! \brief Get current RTC time.
//!
//! \param stores the current time read from RTC.
//!
//! \return none
//
//*****************************************************************************
extern void rtc_getlocal(struct tm* pTime);
#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_RTC_H_ */
