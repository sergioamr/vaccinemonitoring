/*
 * alarm.h
 *
 *  Created on: Mar 19, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_ALARM_H_
#define TEMPSENSOR_V1_ALARM_H_

#ifdef __cplusplus
extern "C" {
#endif

void alarm_low_memory();
void alarm_sms_power_outage();
void alarm_SD_card_failure(char *msg);
SENSOR_STATUS *getAlarmsSensor(int id);

//*****************************************************************************
//! \brief Check for threshold, trigger buzzer and SMS on alarm condition.
//! \param none
//! \return None
//*****************************************************************************
void alarm_monitor();

//*****************************************************************************
//! \brief Check for valid thresholds and set default thresholds in case of
//! invalid values.
//! \param none
//! \return None
//*****************************************************************************
void validatealarmthreshold();

#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_ALARM_H_ */
