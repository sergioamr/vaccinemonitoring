/*
 * uart.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#include "msp430.h"
#include "config.h"
#include "common.h"
#include "uart.h"
#include "stdlib.h"
#include "string.h"
#include "lcd.h"
#define DEBUG_INFO

// We define the exit condition for the wait for ready function
#define WAIT_OK 0
#define WAIT_PROMPT 1
uint8_t uartWaitMode = 0;

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile char RXBuffer[RX_LEN + 1];
char TXBuffer[TX_LEN + 1] ="";
#pragma SET_DATA_SECTION()

volatile static char Ready = 0;

volatile int RXTailIdx = 0;
volatile int RXHeadIdx = 0;

//volatile char* TX;
volatile int TXIdx = 0;
volatile int8_t iTxStop = 0;
volatile uint8_t iError = 0;
volatile uint8_t iTXInProgress = 0;
volatile uint8_t iRXCommandProcessed = 0; // A finished AT command arrived.

int iTxLen = 0;
int iRxLen = RX_LEN;
#define INDEX_2 2
#define INDEX_5 5
#define INDEX_7 7
#define INDEX_9 9
#define INDEX_12 12
#define MIN_OFFSET  0x30
#define MAX_OFFSET 0x39

// Carriage return
#define ATCR 10

#define IMEI_MAX_LEN		 15
//char Substr[TOKEN_LEN+1];

//local functions
static int searchtoken(char* pToken, char** ppTokenPos);
extern void delay(int time);

/*
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG)) {
	case USCI_NONE:
		break;
	case USCI_UART_UCRXIFG:
		if (UCA0STATW & UCRXERR) {
			iError++;
		}
		RXBuffer[RXTailIdx] = UCA0RXBUF;

		//if (RXBuffer[RXTailIdx] == ATCR) {
		//	iRXCommandProcessed = 1;
		//}

		if (RXBuffer[RXTailIdx] == XOFF) {
			iTxStop = 1;
		} else if (RXBuffer[RXTailIdx] == XON) {
			iTxStop = 0;
		}

		RXTailIdx = (RXTailIdx + 1);
		if (RXTailIdx >= iRxLen) {
			RXTailIdx = 0;
		}
		__no_operation();
		break;
	case USCI_UART_UCTXIFG:
		if ((TXIdx < iTxLen) && (iTXInProgress)) {
			UCA0TXBUF = TX[TXIdx];
			TX[TXIdx]='*';
			TXIdx = (TXIdx + 1);
		} else {
			//UCA0IE &= ~UCTXIE;
			iTXInProgress = 0;
		}

		break;
	case USCI_UART_UCSTTIFG:
		break;
	case USCI_UART_UCTXCPTIFG:
		break;
	}
}
*/

void uart_resetbuffer() {
	RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
#ifdef _DEBUG
	memset((char *) RXBuffer, 0, sizeof(RXBuffer));
#endif
}

void uart_init() {
	// Configure USCI_A0 for UART mode
	UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
	UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	// Baud Rate calculation
	// 8000000/(16*115200) = 4.340	//4.340277777777778
	// Fractional portion = 0.340
	// User's Guide Table 21-4: UCBRSx = 0x49
	// UCBRFx = int ( (4.340-4)*16) = 5
	UCA0BRW = 4;                             // 8000000/16/115200
	UCA0MCTLW |= UCOS16 | UCBRF_5 | 0x4900;

	#ifdef LOOPBACK
	UCA0STATW |= UCLISTEN;
	#endif
	UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI

	UCA0IE |= UCRXIE;                // Enable USCI_A0 RX interrupt
}

void sendCommand(const char *cmd) {
	memset((char *) RXBuffer, 0, sizeof(RXBuffer));

	size_t cmdLen = strlen(cmd);
	if (cmd!=TXBuffer) {
		memset((char *) TXBuffer, 0, sizeof(TXBuffer));
		memcpy((char *) TXBuffer, cmd, cmdLen);
	}
	TXIdx = 0;
	RXHeadIdx = RXTailIdx = 0;
	Ready = 0;
	iTxLen = cmdLen;

	UCA0IE |= UCRXIE;   // Enable USCI_A0 RX interrupt
	UCA0IE |= UCTXIE;
	//_BIS_SR(LPM0_bits + GIE);
	_NOP();
	return;
}

#define DELAY_INTERVAL_TIME 100

int waitForReady(uint32_t timeoutTimeMs) {
	int count=timeoutTimeMs/DELAY_INTERVAL_TIME;
	while (count>0) {
		delay(DELAY_INTERVAL_TIME);
		if (Ready==4) {
			delay(30);  // Documentation specifies 30 ms delay between commands
			return 0;
		}
		count--;
	}

	delay(100);  // Documentation specifies 30 ms delay between commands
	return -1;
}

uint8_t iModemErrors=0;

// Try a command until success with timeout and number of attempts to be made at running it
uint8_t uart_tx_timeout(const char *cmd, uint32_t timeout, uint8_t attempts) {

	while(attempts>0) {
		sendCommand(cmd);
		if (!waitForReady(timeout)) {
			return 1;
		}
		attempts--;
		if (g_iDebug_state == 0) {
			lcd_print_debug("MODEM TIMEOUT", LINE2);
		}
	}

	iModemErrors++;
	return 0;
}

void uart_tx_nowait(const char *cmd) {
	sendCommand(cmd);
}

void uart_setPromptMode() {
	uartWaitMode=WAIT_PROMPT;
}

void uart_setOKMode() {
	uartWaitMode=WAIT_OK;
}

uint8_t uart_tx_waitForPrompt(const char *cmd) {
	uart_setPromptMode();
	sendCommand(cmd);
	if (!waitForReady(5000))
		return 1; // We found a prompt

	uart_setOKMode();
	return 0;
}

uint8_t uart_tx(const char *cmd) {
#ifdef _DEBUG
	if (g_iDebug_state == 0) {
		lcd_clear();
		lcd_print_debug((char *) cmd, LINE1);
		delay(30);
	}
#endif
	return uart_tx_timeout(cmd, MODEM_TX_DELAY1, 10);
}

int uart_rx(int atCMD, char* pResponse) {
	return uart_rx_cleanBuf(atCMD, pResponse, 0);
}

// Clears the response buffer if len > 0
int uart_rx_cleanBuf(int atCMD, char* pResponse, uint8_t reponseLen) {
	int ret = -1;
	char* pToken1 = NULL;
	char* pToken2 = NULL;
	int bytestoread = 0;
	int iStartIdx = 0;
	int iEndIdx = 0;

	iRXCommandProcessed = 0;
	if (reponseLen > 0)
		memset(pResponse, 0, reponseLen);

	if (RXHeadIdx < RXTailIdx) {
		pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "CMS ERROR:");
		if (pToken1 != NULL) {
			// ERROR FOUND;
			delay(2000);
			lcd_clear();
			lcd_print_debug((char *) &RXBuffer[RXHeadIdx + 7], LINE1);
			delay(5000);
			return ret;
		} else {
#if defined(DEBUG_INFO) && defined(_DEBUG)
			lcd_print_debug((char *) RXBuffer, LINE2);
			delay(100);
#endif
		}
	}

	//input check
	if (pResponse) {
		switch (atCMD) {
		case ATCMGS:
			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "ERROR");
			if (pToken1!=NULL)
				return UART_ERROR;

			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "+CMGS:");
			if (pToken1!=NULL) {
				strcpy(pResponse, pToken1+6);
				return UART_SUCCESS;
			}
			return UART_FAILED;

		// Getting the SIM Message Center to send it to the backend in order to get the APN
		case ATCMD_CSCA:
			pToken1 = strstr((const char *) RXBuffer, "CSCA:");
			if (pToken1 != NULL) {
				// TODO Parse the format "+CSCA: address,address_type"
				pToken2 = strstr(&pToken1[5], "\""); // Find begin of number on format "Address"
				if (pToken2 == NULL)
					return -1;

				pToken2++;
				pToken1 = strstr(pToken2, "\""); // Find end of number
				if (pToken1 == NULL)
					return -1;

				memcpy(pResponse, pToken2, pToken1 - pToken2);
				return 0;
			}
			break;

		case ATCMD_CCLK:
			pToken1 = strstr((const char *) RXBuffer, "CCLK:");
			if (pToken1 != NULL) {
				pToken2 = strstr(pToken1, "OK");

				if (pToken2 != NULL) {
					bytestoread = pToken2 - pToken1;
					if (bytestoread > CCLK_RESP_LEN) {
						bytestoread = CCLK_RESP_LEN;
					}
				} else {
					bytestoread = CCLK_RESP_LEN;
				}
				memcpy(pResponse, pToken1, bytestoread);
				ret = 0;
			} else {

			}

			break;

		case ATCMD_CSQ:
			pToken1 = strstr((const char *) RXBuffer, "CSQ:");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				pToken2 = strtok(&pToken1[5], ",");

				if (pToken2 != NULL) {
					strncpy(pResponse, pToken2, strlen(pToken2));
				} else {
					pResponse[0] = 0;
				}
				ret = 0;
			}
			break;

		case ATCMD_CPMS:
			pToken1 = strstr((const char *) RXBuffer, "CPMS:");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				pToken2 = strtok(&pToken1[5], ",");

				if (pToken2 != NULL) {
					pToken2 = strtok(NULL, ",");
					strncpy(pResponse, pToken2, strlen(pToken2));
				} else {
					pResponse[0] = 0;
				}
				ret = 0;
			}
			break;

		case ATCMD_CGSN:
			pToken1 = strstr((const char *) RXBuffer, "OK");
			if (pToken1 != NULL) {
				if ((RXBuffer[INDEX_2] >= MIN_OFFSET) && (RXBuffer[INDEX_2] <= MAX_OFFSET)
						&& (RXBuffer[INDEX_5] >= MIN_OFFSET)
						&& (RXBuffer[INDEX_5] <= MAX_OFFSET)
						&& (RXBuffer[INDEX_7] >= MIN_OFFSET)
						&& (RXBuffer[INDEX_7] <= MAX_OFFSET)
						&& (RXBuffer[INDEX_9] >= MIN_OFFSET)
						&& (RXBuffer[INDEX_9] <= MAX_OFFSET)
						&& (RXBuffer[INDEX_12] >= MIN_OFFSET)
						&& (RXBuffer[INDEX_12] <= MAX_OFFSET)) {
					strncpy(pResponse, (const char *) &RXBuffer[2], IMEI_MAX_LEN);
				} else {
					strcpy(pResponse, "INVALID IMEI");
				}
				ret = 0;
			}						//	break;
		case ATCMD_HTTPSND:
			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], ">>>");
			if (pToken1 != NULL) {
				//bytestoread = &RXBuffer[RXTailIdx] - pToken1;
				//if(bytestoread > HTTPSND_RSP_LEN)
				//{
				//bytestoread = HTTPSND_RSP_LEN;
				//}
				//memcpy(pResponse, pToken1, bytestoread);

				//need to esnure that RXTailIdx is not moving. i.e disable any asynchronous
				//notification
				if (((RXTailIdx > RXHeadIdx) && (pToken1 < &RXBuffer[RXTailIdx]))
						|| (RXTailIdx < RXHeadIdx)) {
					strcpy(pResponse, ">>>");
					RXHeadIdx = RXTailIdx;
					ret = 0;
				}
			} else if (RXTailIdx < RXHeadIdx) {
				//rollover
				pToken1 = strstr((const char *) RXBuffer, ">>>");
				if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
					strcpy(pResponse, ">>>");
					RXHeadIdx = RXTailIdx;
					ret = 0;
				}
				//check http post response is splitted
				else if (((RXBuffer[iRxLen - 2] == '>') && (RXBuffer[iRxLen - 1] == '>')
						&& (RXBuffer[0] == '>'))
						|| ((RXBuffer[iRxLen - 1] == '>') && (RXBuffer[0] == '>')
								&& (RXBuffer[1] == '>'))) {
					strcpy(pResponse, ">>>");
					RXHeadIdx = RXTailIdx;
					ret = 0;
				} else {
					pResponse[0] = 0;
				}

			}
			break;

		case ATCMD_CMGR:
			//check if this msg is unread
			iStartIdx = 0;
			pToken1 = strstr((const char *) RXBuffer, "UNREAD");
			//pToken1 = strstr(RXBuffer,"READ");
			//only STATUS and RESET are supported
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				pToken1 = strstr((const char *) RXBuffer, "STATUS");
				if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
					strncpy(pResponse, "1,", 2);
					iStartIdx = 1;	//indicates that msg is valid
				} else {
					pToken1 = strstr((const char *) RXBuffer, "RESET");
					if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
						strncpy(pResponse, "2,", 2);
						iStartIdx = 1; //indicates that msg is valid
					}
				}
			}

			if (iStartIdx) {
				iEndIdx = 0;
				pToken1 = strtok((char *) RXBuffer, ",");
				while (pToken1) {
#if 0
					iEndIdx++;
					if(iEndIdx == 9) //10th field is sender's phone number
					{
						break;
					}
					else
					{
						pToken1 = strtok(NULL,",");
					}
#endif
					pToken2 = strstr(pToken1, "+91");
					if (pToken2) {
						break;
					}
					pToken1 = strtok(NULL, ",");

				}
				//check if not null, this token contains the senders phone number
				if (pToken1) {
					strncpy(&pResponse[2], pToken1, PHONE_NUM_LEN);
				} else {
					pResponse[0] = 0; //ingore the message
				}

			}
			break;

		case ATCMD_CMGL:
			//check if this msg is unread
			pToken1 = strstr((const char *) RXBuffer, "UNREAD");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				if (searchtoken("$ST", &pToken1) == 0) {
					RXHeadIdx = pToken1 - &RXBuffer[0];
					iStartIdx = RXHeadIdx;

					//memset(Substr,0,sizeof(Substr));
					//strcpy(Substr,"$EN");
					if (searchtoken("$EN", &pToken2) == 0) {
						RXHeadIdx = pToken2 - &RXBuffer[0];
						iEndIdx = RXHeadIdx;

						if (iEndIdx > iStartIdx) {
							//no rollover
							bytestoread = iEndIdx - iStartIdx;
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, (const char *) &RXBuffer[iStartIdx],
									bytestoread);
							ret = 0;
						} else {
							//rollover
							bytestoread = iRxLen - iStartIdx;
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, (const char *) &RXBuffer[iStartIdx],
									bytestoread);
							memcpy(&pResponse[bytestoread], (const char *) RXBuffer,
									iEndIdx);
							ret = 0;
						}
					}
				}
			} else {
				//no start tag found
				pResponse[0] = 0;
			}
			break;

		case ATCMD_HTTPRCV:
			pToken1 = strstr((const char *) RXBuffer, "HTTPRING");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				if (searchtoken("$ST", &pToken1) == 0) {
					RXHeadIdx = pToken1 - &RXBuffer[0];
					iStartIdx = RXHeadIdx;

					//memset(Substr,0,sizeof(Substr));
					//strcpy(Substr,"$EN");
					if (searchtoken("$EN", &pToken2) == 0) {
						RXHeadIdx = pToken2 - &RXBuffer[0];
						iEndIdx = RXHeadIdx;

						if (iEndIdx > iStartIdx) {
							//no rollover
							bytestoread = iEndIdx - iStartIdx;
							if (bytestoread > HTTPRCV_RSP_LEN) {
								bytestoread = HTTPRCV_RSP_LEN;
							}
							memcpy(pResponse, (const char *) &RXBuffer[iStartIdx],
									bytestoread);
							ret = 0;
						} else {
							//rollover
							bytestoread = iRxLen - iStartIdx;
							if (bytestoread > HTTPRCV_RSP_LEN) {
								bytestoread = HTTPRCV_RSP_LEN;
							}
							memcpy(pResponse, (const char *) &RXBuffer[iStartIdx],
									bytestoread);
							memcpy(&pResponse[bytestoread], (const char *) RXBuffer,
									iEndIdx);
							ret = 0;
						}
					}
				}
			} else {
				//no start tag found
				pResponse[0] = 0;
			}

			break;
		case ATCMD_CPIN:
			pToken1 = strstr((const char *) RXBuffer, "OK");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
			} else {
				pToken1 = strstr((const char *) RXBuffer, "SIM");
			}
/*
		case ATCMD_HTTPQRY:
			pToken1 = strstr(RXBuffer, "HTTPRING");
			if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
				if (searchtoken("$ST", &pToken1) == 0) {
					RXHeadIdx = pToken1 - &RXBuffer[0];
					iStartIdx = RXHeadIdx;

					//memset(Substr,0,sizeof(Substr));
					//strcpy(Substr,"$EN");
					if (searchtoken("$EN", &pToken2) == 0) {
						RXHeadIdx = pToken2 - &RXBuffer[0];
						iEndIdx = RXHeadIdx;

						if (iEndIdx > iStartIdx) {
							//no rollover
							bytestoread = iEndIdx - iStartIdx;
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, &RXBuffer[iStartIdx], bytestoread);
							ret = 0;
						} else {
							//rollover
							bytestoread = iRxLen - iStartIdx;
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, &RXBuffer[iStartIdx], bytestoread);
							memcpy(&pResponse[bytestoread], RXBuffer, iEndIdx);
							ret = 0;
						}
					}
				}
			}
			break;
*/
		default:
			break;
		}
	}
	return ret;
}

int searchtoken(char* pToken, char** ppTokenPos) {
	int ret = -1;
	char* pToken1 = NULL;

	pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], pToken);
	if (pToken1 != NULL) {
		//need to esnure that RXTailIdx is not moving. i.e disable any asynchronous
		//notification
		if (((RXTailIdx > RXHeadIdx) && (pToken1 < &RXBuffer[RXTailIdx]))
				|| (RXTailIdx < RXHeadIdx)) {
			*ppTokenPos = pToken1;
			ret = 0;
		}
	} else if (RXTailIdx < RXHeadIdx) {
		//rollover
		pToken1 = strstr((const char *) RXBuffer, pToken);
		if ((pToken1 != NULL) && (pToken1 < &RXBuffer[RXTailIdx])) {
			*ppTokenPos = pToken1;
			ret = 0;
		} else {
		}

	}
	//check http post response is splitted
	else if ((RXBuffer[iRxLen - 2] == pToken[0]) && (RXBuffer[iRxLen - 1] == pToken[1])
			&& (RXBuffer[0] == pToken[2]))

			{
		*ppTokenPos = (char *) &RXBuffer[iRxLen - 2];
		ret = 0;
	} else if ((RXBuffer[iRxLen - 1] == pToken[0]) && (RXBuffer[0] == pToken[1])
			&& (RXBuffer[1] == pToken[2])) {
		*ppTokenPos = (char *) &RXBuffer[iRxLen - 1];
		ret = 0;
	}

	return ret;
}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(UCA0IV, USCI_SPI_UCTXIFG)) {
	case USCI_NONE:
		break;

	case USCI_SPI_UCRXIFG:
		if (RXTailIdx >= sizeof(RXBuffer))
			RXTailIdx = 0;

		RXBuffer[RXTailIdx++] = UCA0RXBUF;

		if (uartWaitMode==WAIT_OK) {
			if (Ready == 0 && UCA0RXBUF == 'O') Ready++; else
			if (Ready == 1 && UCA0RXBUF == 'K') Ready++; else
			if (Ready == 2 && UCA0RXBUF == 0x0D) Ready++; else
			if (Ready == 3 && UCA0RXBUF == 0x0A) Ready++; else
			if (Ready < 4) Ready = 0;
		} else {
			if (UCA0RXBUF == 0x0D) Ready++; else
			if (UCA0RXBUF == 0x0A) Ready++; else
			if (UCA0RXBUF == '>' && Ready==2)
				Ready=4;
		}

		break;

	case USCI_SPI_UCTXIFG:
		UCA0TXBUF = TXBuffer[TXIdx];               // Transmit characters
#ifdef _DEBUG
		//TXBuffer[iTxPos] = '*';
#endif

		if (TXIdx >= iTxLen) {
			TXIdx = 0;
			UCA0IE &= ~UCTXIE; 	// Finished transmision
		}

		TXIdx++;
		break;

	default:
		break;
	}
}

