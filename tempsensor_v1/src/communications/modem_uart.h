/*
 * uart.h
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_UART_H_
#define TEMPSENSOR_V1_UART_H_

#define TX_LEN   			200
#define RX_LEN   			512

typedef struct {
	int8_t iActive;

	uint16_t iRXTailIdx;
	uint16_t iRXHeadIdx;

	char bWaitForTXEnd;
	char bTransmissionEnd;

	int8_t iUartState;
	uint8_t bRXWaitForReturn;

	uint16_t iTXIdx;

	uint8_t iError;

	uint16_t iRxCountBytes;
	uint16_t iTxCountBytes;
	uint16_t iTxLen;

	char szOK[16];
	uint8_t OKIdx;
	int8_t OKLength;

	char szError[16];
	uint8_t ErrorIdx;
	int8_t ErrorLength;

} UART_TRANSFER;

#define UART_SUCCESS 0
#define UART_ERROR -1
#define UART_FAILED 1
#define UART_TIMEOUT 2
#define UART_INPROCESS 3

#include <msp430.h>
#ifdef __cplusplus
extern "C"
{
#endif

const char *uart_getRXHead();

void uart_setOKMode();

void uart_setupIO_clock(); // Initializes the speed and the bauds of the UCA0
void uart_setHTTPPromptMode();
void uart_setSMSPromptMode();
void uart_setHTTPDataMode();

//*****************************************************************************
// Set tail and head to 0 for easy parsing.
//*****************************************************************************
void uart_resetbuffer();
void uart_setupIO();

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

extern char modem_lastCommand[16];

#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_UART_H_ */
