/*
 * sms.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_SMS_H_
#define TEMPSENSOR_V1_SMS_H_

//*****************************************************************************
//! \brief send sms msg
//! \param pointer to sms text contents
//! \return UART_SUCCESS or UART_ERROR
//*****************************************************************************
extern uint8_t sendmsg(char* pData);
extern uint8_t sendmsg_number(char *szPhoneNumber, char* pData);

//*****************************************************************************
//! \brief receive sms msg
//! \param pointer to store the received sms text, if pData[0] is 0 then no msg
//! 	   is available for reading
//! \return 0 on success, -1 on failure
//*****************************************************************************
extern int recvmsg(int8_t iMsgIdx,char* pData);

//*****************************************************************************
//! \brief delete the sms msg(s) read and sent successfully
//! \param none
//! \return none
//*****************************************************************************
extern void delreadmsg();

extern void delallmsg();
extern void delmsg(int8_t iMsgIdx, char* pData);


#endif /* TEMPSENSOR_V1_SMS_H_ */
