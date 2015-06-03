/*
 * modem.h
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_MODEM_H_
#define TEMPSENSOR_V1_MODEM_H_

#include "command_timeout.h"
#include "modem_errors.h"

extern char ctrlZ[2];
extern char ESC[2];

/*********************************************************************************/
/* NETWORK STATUS 															     */
/*********************************************************************************/
// Mode 1 network status

#define NETWORK_STATUS_NOT_REGISTERED_NOT_LOOKING 0
#define NETWORK_STATUS_REGISTERED_HOME_NETWORK 	  1
#define NETWORK_STATUS_NOT_REGISTERED_SEARCHING   2
#define NETWORK_STATUS_REGISTRATION_DENIED	      3
#define NETWORK_STATUS_UNKNOWN	      			  4
#define NETWORK_STATUS_REGISTERED_ROAMING 		  5

#define NETWORK_MODE_2_ENABLED
#define NETWORK_MODE_1_NOT_REGISTERED_NOT_LOOKING 0

/*********************************************************************************/
/* MODEM FUNCTIONALITY														     */
/*********************************************************************************/

#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode();
int8_t modem_exit_powersave_mode();
#endif

extern void modem_checkSignal();
extern void modem_init();
extern void modem_getExtraInfo();
extern void modem_survey_network();
extern void modem_swap_SIM();
extern void modem_pull_time();
extern int8_t modem_first_init();
extern void modem_getSMSCenter();
extern void modem_set_max_messages();

extern void modem_setNumericError(char errorToken, int16_t errorCode);
extern uint16_t modem_parse_error(const char *error);
extern void modem_check_uart_error();

#endif /* TEMPSENSOR_V1_MODEM_H_ */
