/*
 * http.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_HTTP_H_
#define TEMPSENSOR_V1_HTTP_H_

void backend_get_configuration();

int8_t http_setup();
void http_deactivate();
int8_t http_enable();
int http_get_configuration();
int http_check_error(int *retry);
int http_open_connection(int data_length);
void backend_get_configuration();
int8_t http_post(char* postdata);
int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize);

int http_post_gprs_connection_status(char status);
int http_post_sms_status(void);

extern const char HTTP_INCOMING_DATA[];

#endif /* TEMPSENSOR_V1_HTTP_H_ */
