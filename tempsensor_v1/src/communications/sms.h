/*
 * sms.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_SMS_H_
#define TEMPSENSOR_V1_SMS_H_

void sms_send_data_request(char *number);
int8_t sms_process_msg(char* pSMSmsg);
//*****************************************************************************
//! \brief send sms msg
//! \param pointer to sms text contents
//! \return UART_SUCCESS or UART_ERROR
//*****************************************************************************
uint8_t sms_send_message(char* pData);
uint8_t sms_send_message_number(char *szPhoneNumber, char* pData);

#define MAX_SMS_SIZE 160

// + MODEM EXTRA DATA FOR SENDING + END
#define MAX_SMS_SIZE_FULL MAX_SMS_SIZE + 4

//*****************************************************************************
//! \brief delete the sms msg(s) read and sent successfully
//! \param none
//! \return none
//*****************************************************************************
void delreadmsg();

void delallmsg();
void delmsg(int8_t iMsgIdx);

void sms_send_heart_beat();
int8_t sms_process_messages();

#endif /* TEMPSENSOR_V1_SMS_H_ */
