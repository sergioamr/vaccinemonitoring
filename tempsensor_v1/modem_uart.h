/*
 * uart.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_UART_H_
#define TEMPSENSOR_V1_UART_H_

#define TX_LEN   			512
#define RX_LEN   			2048
#define TOKEN_LEN			3

#define ATCMD_HTTPSND		1
#define ATCMD_CMGL			2
#define ATCMD_CNUM 			3
#define ATCMD_CSQ			4
#define ATCMD_CGSN			5
#define ATCMD_CPMS_CURRENT	6
#define ATCMD_CMGR			7
#define ATCMD_CPIN          8
#define ATCMD_CSCA 			9
#define ATCMD_BND			10
#define ATCMD_CMGS			11
#define ATCMD_HTTPQRY       12
#define ATCMD_CPMS_MAX		15

#define UART_SUCCESS 0
#define UART_ERROR -1
#define UART_FAILED 1
#define UART_TIMEOUT 2
#define UART_INPROCESS 3

#define CCLK_RESP_LEN		28
#define HTTPSND_RSP_LEN		20
#define HTTPRCV_RSP_LEN		sizeof(RXBuffer)
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
extern size_t iRxLen;
extern volatile char RXBuffer[RX_LEN];

void uart_setOKMode();

void uart_setupIO_clock(); // Initializes the speed and the bauds of the UCA0
void uart_setHTTPPromptMode();
void uart_setSMSPromptMode();

//*****************************************************************************
// Set tail and head to 0 for easy parsing.
//*****************************************************************************
void uart_resetbuffer();
void uart_setupIO();

int searchtoken(char* pToken, char** ppTokenPos);

//*****************************************************************************
//! \brief Returns the state of the last transaction
//! \param pointer to transmit buffer
//! \return UART_SUCCESS UART_ERROR or UART_TIMEOUT
//*****************************************************************************
int8_t uart_getTransactionState();

void uart_setCheckMsg(const char *szOK, const char *szError);

void uart_setNumberOfPages(int numPages);
void uart_setRingBuffer();

void uart_setDefaultIntervalDivider();
void uart_setDelayIntervalDivider(uint8_t divider);


//*****************************************************************************
//! \brief Transmit to UART
//! \param pointer to transmit buffer
//! \return 0 on success, -1 on failure
//*****************************************************************************
uint8_t uart_tx(const char* pTxData);
uint8_t uart_tx_timeout(const char *cmd, uint32_t timeout, uint8_t attempts);
void uart_tx_nowait(const char *cmd);
uint8_t uart_tx_waitForPrompt(const char *cmd, uint32_t promptTime);
uint8_t uart_txf(const char *_format, ...);

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
int uart_rx(int atCMD, char* pResponse);
int uart_rx_cleanBuf(int atCMD, char* pResponse, uint16_t reponseLen);

#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_UART_H_ */
