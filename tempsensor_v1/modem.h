/*
 * modem.h
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_MODEM_H_
#define TEMPSENSOR_V1_MODEM_H_

#include "command_timeout.h"
extern char ctrlZ[2];
extern char ESC[2];


#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode();
int8_t modem_exit_powersave_mode();
#endif

extern void modem_checkSignal();
extern void modem_init();
extern void modem_getExtraInfo();
extern void modem_survey_network();
extern void modem_swapSIM();
extern void modem_pull_time();
extern int8_t modem_first_init();

#endif /* TEMPSENSOR_V1_MODEM_H_ */
