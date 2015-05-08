/*
 * alarm.h
 *
 *  Created on: Mar 19, 2015
 *      Author: rajeevnx
 */

#ifndef ALARM_H_
#define ALARM_H_

#ifdef __cplusplus
extern "C"
{
#endif


//*****************************************************************************
//
//! \brief Check for threshold, trigger buzzer and SMS on alarm condition.
//!
//! \param none
//!
//! \return None
//
//*****************************************************************************
extern void monitoralarm();

//*****************************************************************************
//
//! \brief Check for valid thresholds and set default thresholds in case of
//! invalid values.
//!
//! \param none
//!
//! \return None
//
//*****************************************************************************
extern void validatealarmthreshold();

#ifdef __cplusplus
}
#endif

#endif /* ALARM_H_ */
