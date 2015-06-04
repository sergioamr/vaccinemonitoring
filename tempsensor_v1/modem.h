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

void modem_checkSignal();
void modem_init();
void modem_getExtraInfo();
void modem_survey_network();
void modem_swap_SIM();
void modem_pull_time();
int8_t modem_first_init();
void modem_getSMSCenter();
void modem_set_max_messages();

void modem_setNumericError(char errorToken, int16_t errorCode);
uint16_t modem_parse_error(const char *error);
void modem_check_uart_error();

/*********************************************************************************/
/* PARSING TOOLS  																 */
/*********************************************************************************/

// Parsing macros helpers

#define PARSE_FINDSTR_RET(token, sz, error) token=strstr((const char *) &RXBuffer[RXHeadIdx], sz); \
	if(token==NULL) return error; else token+=strlen(sz)-1;

#define PARSE_FIRSTVALUE(token, var, delimiter, error) token = strtok(token, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

#define PARSE_NEXTVALUE(token, var, delimiter, error) token = strtok(NULL, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

#define PARSE_SKIP(token, delimiter, error) token = strtok(NULL, delimiter); \
	if(token==NULL) return error;

#endif /* TEMPSENSOR_V1_MODEM_H_ */
