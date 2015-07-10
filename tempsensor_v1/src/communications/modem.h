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

#define NETWORK_ERROR_SIGNAL_FAILURE			  101

#define NETWORK_MODE_2_ENABLED
#define NETWORK_MODE_1_NOT_REGISTERED_NOT_LOOKING 0

/*********************************************************************************/
/* NETWORK SERVICE    														     */
/*********************************************************************************/

// Not yet initialized
#define NETWORK_NOT_SELECTED -1

// Global System for Mobile Communications (2G)
#define NETWORK_GSM 0

// General packet radio service
#define NETWORK_GPRS 1

/*********************************************************************************/
/* MODEM FUNCTIONALITY														     */
/*********************************************************************************/

#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode();
int8_t modem_exit_powersave_mode();
#endif

uint8_t modem_check_working_SIM();
int modem_getSignal();
void modem_init();
void modem_getExtraInfo();
void modem_survey_network();
void modem_check_sim_active();
int modem_swap_to_SIM(int sim);
int modem_swap_SIM();
const char *modem_getNetworkServiceCommand();
void modem_network_sequence();
int modem_getNetworkService();
void modem_setNetworkService(int service);
int modem_connect_network(uint8_t attempts);
void modem_pull_time();
int8_t modem_first_init();
int8_t modem_check_network();
int8_t modem_getSMSCenter();
int8_t modem_set_max_messages();

void modem_setNumericError(char errorToken, int16_t errorCode);
uint16_t modem_parse_error(const char *error);
void modem_check_uart_error();
void modem_ignore_next_errors(int errors);

/*********************************************************************************/
/* PARSING TOOLS  																 */
/*********************************************************************************/

// Parsing macros helpers

#define PARSE_FINDSTR_BUFFER_RET(token, buffer, sz, error) token=strstr((const char *) buffer, sz); \
	if(token==NULL) return error; else token+=strlen(sz)-1;

#define PARSE_FINDSTR_RET(token, sz, error) token=strstr((const char *) uart_getRXHead(), sz); \
	if(token==NULL) return error; else token+=strlen(sz)-1;

#define PARSE_FIRSTVALUE(token, var, delimiter, error) token = strtok(token, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

#define PARSE_NEXTVALUE(token, var, delimiter, error) token = strtok(NULL, delimiter); \
	if(token!=NULL) *var = atoi(token); else return error;

#define PARSE_FIRSTSTRING(token, var, length, delimiter, error) token = strtok(token, delimiter); \
	if(token!=NULL) strncpy(var, token, length); else return error;

#define PARSE_NEXTSTRING(token, var, length, delimiter, error) token = strtok(NULL, delimiter); \
	if(token!=NULL) strncpy(var, token, length); else return error;

#define PARSE_FIRSTVALUECOMPARE(token, var, match, error) token = strtok(token, delimiter); \
	if(token!=NULL) { if(var!=atoi(token)) { var=atoi(token); } else { return match; } } else return error;

#define PARSE_NEXTVALUECOMPARE(token, var, match, error) token = strtok(NULL, delimiter); \
	if(token!=NULL) { if(var!=atoi(token)) var=atoi(token); } else return error;

#define PARSE_FIRSTSKIP(token, delimiter, error) token = strtok(token, delimiter); \
	if(token==NULL) return error; else token+=strlen(token)-1;

#define PARSE_SKIP(token, delimiter, error) token = strtok(NULL, delimiter); \
	if(token==NULL) return error;

//else token+=strlen(sz)-1;

#endif /* TEMPSENSOR_V1_MODEM_H_ */
