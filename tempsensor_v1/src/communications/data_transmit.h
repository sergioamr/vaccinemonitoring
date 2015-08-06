/*
 * data_transmit.h
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#ifndef DATA_TRANSMIT_H_
#define DATA_TRANSMIT_H_

int8_t data_transmit(uint8_t *pSampleCnt);
int8_t sync_send_http();
int8_t data_send_sms();
void process_batch();

#endif /* DATA_TRANSMIT_H_ */
