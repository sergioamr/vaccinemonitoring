/*
 * sms.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_SMS_H_
#define TEMPSENSOR_V1_SMS_H_

// + 0 + ctrZ
#define SMS_MAX_SIZE 160+1+2

void sms_send_data_request(char *number);
int8_t sms_process_msg(char* pSMSmsg);
//*****************************************************************************
//! \brief send sms msg
//! \param pointer to sms text contents
//! \return UART_SUCCESS or UART_ERROR
//*****************************************************************************
uint8_t sms_send_message(char* pData);
uint8_t sms_send_message_number(char *szPhoneNumber, char* pData);

//*****************************************************************************
//! \brief receive sms msg
//! \param pointer to store the received sms text, if pData[0] is 0 then no msg
//! 	   is available for reading
//! \return 0 on success, -1 on failure
//*****************************************************************************
int sms_recv_message(int8_t iMsgIdx,char* pData);

//*****************************************************************************
//! \brief delete the sms msg(s) read and sent successfully
//! \param none
//! \return none
//*****************************************************************************
void delreadmsg();

void delallmsg();
void delmsg(int8_t iMsgIdx, char* pData);

void sms_send_heart_beat();
int8_t sms_process_messages();

#endif /* TEMPSENSOR_V1_SMS_H_ */
