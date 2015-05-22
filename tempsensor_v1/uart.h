/*
 * uart.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef UART_H_
#define UART_H_

#define TX_LEN   			300
#define RX_LEN   			102
#define TOKEN_LEN			3

#define ATCMD_CCLK			0
#define ATCMD_HTTPSND		1
#define ATCMD_CMGL			2
#define ATCMD_HTTPRCV		3
#define ATCMD_CSQ			4
#define ATCMD_CGSN			5
#define ATCMD_CPMS			6
#define ATCMD_CMGR			7
#define ATCMD_CPIN          8

#define CCLK_RESP_LEN		28
#define HTTPSND_RSP_LEN		20
#define CMGL_RSP_LEN		140
#define HTTPRCV_RSP_LEN		CFG_SIZE
#define CGSN_OFFSET			4
#define CGSN_LEN			15

#define XOFF				0x13
#define XON					0x11

#include <msp430.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern volatile int RXTailIdx;
extern volatile int RXHeadIdx;
extern int iRxLen;
extern volatile char RX[RX_LEN+1];

//*****************************************************************************
//
//! \brief Transmit to UART
//!
//! \param pointer to transmit buffer
//!
//! \return 0 on success, -1 on failure
//
//*****************************************************************************
extern int uart_tx(volatile char* pTxData);

//*****************************************************************************
//
//! \brief Receive from UART
//!
//! \param AT command code
//! \param pointer to AT command response
//!
//! \return 0 on success, -1 on failure
//
//*****************************************************************************
extern int uart_rx(int atCMD, char* pResponse);

#ifdef __cplusplus
}
#endif

#endif /* UART_H_ */
