/*
 * uart.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef UART_H_
#define UART_H_

#define TX_LEN   			360
#define RX_LEN   			360
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
#define ATCMD_CSCA 			9
#define ATCMD_BND			10
#define ATCMD_CMGS			11
#define ATCMD_HTTPQRY       12
#define ATCMD_CSURVC		13

#define UART_SUCCESS 0
#define UART_ERROR -1
#define UART_FAILED 1

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
extern volatile char RXBuffer[RX_LEN+1];

extern void uart_setOKMode();
extern uint8_t uart_tx_waitForPrompt(const char *cmd);

extern void uart_setClock(); // Initializes the speed and the bauds of the UCA0
extern void uart_setPromptMode();

//*****************************************************************************
// Set tail and head to 0 for easy parsing.
//*****************************************************************************
extern void uart_resetbuffer();
extern void uart_setIO();

//*****************************************************************************
//
//! \brief Transmit to UART
//!
//! \param pointer to transmit buffer
//!
//! \return 0 on success, -1 on failure
//
//*****************************************************************************
extern uint8_t uart_tx(const char* pTxData);
extern uint8_t uart_tx_timeout(const char *cmd, uint32_t timeout, uint8_t attempts);
extern void uart_tx_nowait(const char *cmd);

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
extern int uart_rx_cleanBuf(int atCMD, char* pResponse, uint16_t reponseLen);

#ifdef __cplusplus
}
#endif

#endif /* UART_H_ */
