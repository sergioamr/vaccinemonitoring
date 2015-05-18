/*
 * sms.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef SMS_H_
#define SMS_H_

#ifndef EXTERN
#define EXTERN extern
#endif

//*****************************************************************************
//
//! \brief send sms msg
//!
//! \param pointer to sms text contents
//!
//! \return none
//
//*****************************************************************************
EXTERN void sendmsg(char* pData);

//*****************************************************************************
//
//! \brief receive sms msg
//!
//! \param pointer to store the received sms text, if pData[0] is 0 then no msg
//! 	   is available for reading
//!
//! \return 0 on success, -1 on failure
//
//*****************************************************************************
EXTERN int recvmsg(int8_t iMsgIdx,char* pData);

//*****************************************************************************
//
//! \brief delete the sms msg(s) read and sent successfully
//!
//! \param none
//!
//! \return none
//
//*****************************************************************************
EXTERN void delreadmsg();

#endif /* SMS_H_ */
