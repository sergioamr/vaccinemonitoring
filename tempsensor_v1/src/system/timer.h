/*
 * timer.h
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_TIMER_H_
#define TEMPSENSOR_V1_TIMER_H_

#include <msp430.h>

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! \brief delay loop
//!
//! \param time in milliseconds (shoud be in multiple of 10)
//!
//! \return after the expiry of specified time
//
//*****************************************************************************
extern void delay(uint32_t time);

//*****************************************************************************
//
//! \brief delay loop
//!
//! \param time in microseconds (shoud be in multiple of 250 us)
//!
//! \return after the expiry of specified time
//
//*****************************************************************************
extern void delayus(int time);

#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_TIMER_H_ */
