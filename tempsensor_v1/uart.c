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
#include "globals.h"
#include "stdio.h"
#include "string.h"

// AT Messages returns to check.
char AT_MSG_OK[]={ 0x0D, 0x0A, 'O', 'K', 0x0D, 0x0A, 0 };
char AT_MSG_PROMPT[]={ 0x0D, 0x0A, '>', 0};
char AT_CME_ERROR[]= "+CME ERROR:";

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile char RXBuffer[RX_LEN + 1];
char TXBuffer[TX_LEN + 1] ="";
#pragma SET_DATA_SECTION()

volatile static char TransmissionEnd = 0;

volatile int RXTailIdx = 0;
volatile int RXHeadIdx = 0;

volatile int g_iUartState = 0;
volatile int g_iRxCountBytes = 0;
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

//local functions
static int searchtoken(char* pToken, char** ppTokenPos);
extern void delay(int time);

void uart_resetbuffer() {
	RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
#ifdef _DEBUG
	memset((char *) RXBuffer, 0, sizeof(RXBuffer));
#endif
}

/*************************** ERROR AND OK  *****************************/
// Function to check end of msg

char g_szOK[32];
uint8_t OKIdx=0;
int8_t OKLength=0;

char g_szError[32];
uint8_t ErrorIdx=0;
int8_t ErrorLength=0;

void uart_setCheckMsg(const char *szOK, const char *szError) {
	TransmissionEnd=0;
	g_iUartState=UART_INPROCESS;

	if (szError!=NULL) {
		ErrorIdx=0;
		ErrorLength=strlen(szError)-1;  // We don't check for 0 terminated string
		strcpy(g_szError, szError);
	} else
		ErrorLength=-1;

	if (szOK!=NULL) {
		OKIdx=0;
		OKLength=strlen(szOK)-1; // We don't check for 0 terminated string
		strcpy(g_szOK, szOK);
	} else
		OKLength=-1;

}

// We define the exit condition for the wait for ready function
inline void uart_checkERROR() {

	if (ErrorLength==-1)
		return;

	if (UCA0RXBUF!=g_szError[++ErrorIdx]) {
		ErrorIdx=0;
		return;
	}

	if (ErrorIdx==ErrorLength) {
		TransmissionEnd=1;
		g_iUartState=UART_ERROR;
		ErrorIdx=0;
	}
}


volatile int uart_numDataPages = -1; // Number of pages of data to retrieve. -1 is circular buffer

// Cancels data transaction after a number of pages has been obtained
void uart_setNumberOfPages(int numPages) {
	uart_numDataPages=numPages;
}

void uart_setRingBuffer() {
	uart_numDataPages=-1;
}

// We define the exit condition for the wait for ready function
inline void uart_checkOK() {

	if (OKLength==-1)
		return;

	if (UCA0RXBUF!=g_szOK[++OKIdx]) {
		OKIdx=0;
		return;
	}

	if (OKIdx==OKLength) {
		TransmissionEnd=1;
		g_iUartState=UART_SUCCESS;
		OKIdx=0;
	}
}

void uart_setIO() {
	// P2.0 UCA0SIMO
	// P2.1 UCA0RXD/ UCA0SOMI

	P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
	P2SEL0 &= ~(BIT0 | BIT1);
	P4DIR |= BIT0 | BIT5 | BIT6 | BIT7; // Set P4.0 (Modem reset), LEDs to output direction

	// Turn off modem
	P4OUT |= BIT0;
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

uint8_t uart_getTransactionState() {
	return g_iUartState;
}

// Reset all buffers, headers, and counters for transmission
void uart_resetHeaders() {

	memset((char *) RXBuffer, 0, sizeof(RXBuffer));

	TXIdx = 0;
	RXHeadIdx = RXTailIdx = 0;
	TransmissionEnd = 0;

	OKIdx=0;     // Indexes for checking for end of command
	ErrorIdx=0;
	g_iUartState=UART_INPROCESS;  // Setup in process, we are actually going to generate a command

	g_iRxCountBytes=0; // Stats to know how much data are we getting on this buffer.
}

void sendCommand(const char *cmd) {

	// Clear reset
	uart_resetHeaders();

	// Only copy the buffer if we are not actually using TXBuffer,
	// some places of the old code use the TXBuffer directly :-/

	iTxLen = strlen(cmd);

	// Store the maximum size used from this buffer
	if (iTxLen>g_pSysCfg->maxTXBuffer)
		g_pSysCfg->maxTXBuffer=iTxLen;

	if (iTxLen>sizeof(TXBuffer)) {
		lcd_clear();
		lcd_print("TXBUFFER ERROR");
		delay(10000);
	}

	if (cmd!=TXBuffer) {
		memset((char *) TXBuffer, 0, sizeof(TXBuffer));
		memcpy((char *) TXBuffer, cmd, iTxLen);
	}

	UCA0IE |= UCRXIE;   // Enable USCI_A0 RX interrupt
	UCA0IE |= UCTXIE;
	//_BIS_SR(LPM0_bits + GIE);
	_NOP();
	return;
}

#define DEFAULT_INTERVAL_DELAY 10
uint8_t delayDivider = DEFAULT_INTERVAL_DELAY;
void uart_setDefaultIntervalDivider() {
	delayDivider = DEFAULT_INTERVAL_DELAY;
}

void uart_setDelayIntervalDivider(uint8_t divider) {
	delayDivider = divider;
}

int waitForReady(uint32_t timeoutTimeMs) {
	uint32_t delayTime = timeoutTimeMs/delayDivider;
	int count=delayDivider;
	while (count>0) {
		delay(delayTime);
		if (TransmissionEnd==1) {
			delay(30);  // Documentation specifies 30 ms delay between commands
			return UART_SUCCESS; // There was a transaction, you have to check the state of the uart transaction to check if it was successful
		}
		count--;
	}

	// Store the maximum size used from this buffer
	if (g_iRxCountBytes>g_pSysCfg->maxRXBuffer)
		g_pSysCfg->maxRXBuffer=g_iRxCountBytes;

	delay(100);  // Documentation specifies 30 ms delay between commands

	if (g_iUartState!=UART_SUCCESS) {
		g_iUartState=UART_TIMEOUT;
		return UART_FAILED;
	}

	return UART_SUCCESS;
}

uint8_t iModemErrors=0;

// Try a command until success with timeout and number of attempts to be made at running it
uint8_t uart_tx_timeout(const char *cmd, uint32_t timeout, uint8_t attempts) {

	while(attempts>0) {
		sendCommand(cmd);
		if (!waitForReady(timeout)) {
			uart_setRingBuffer();
			return UART_SUCCESS;
		}
		attempts--;
		if (g_iLCDVerbose == VERBOSE_BOOTING) {
			lcd_print_progress("MODEM TIMEOUT", LINE2);
		}
	}

	// If there was an error we have to wait a bit to retrieve everything that is left from the transaction, like the error message
	int uart_state = uart_getTransactionState();
	if (uart_state==UART_ERROR)
		delay(500);

	iModemErrors++;
	uart_setRingBuffer(); // Restore the ring buffer if it was changed.
	return UART_FAILED;
}

void uart_tx_nowait(const char *cmd) {
	sendCommand(cmd);
}

uint8_t uart_tx_waitForPrompt(const char *cmd) {
	uart_setPromptMode();
	sendCommand(cmd);
	if (!waitForReady(5000)) {
		uart_setOKMode();
		return 1; // We found a prompt
	}

	uart_setOKMode();
	return 0;
}

uint32_t g_iModemMaxWait = MODEM_TX_DELAY1;

void uart_setDelay(uint32_t delay) {
	g_iModemMaxWait=MODEM_TX_DELAY1;
}

uint8_t isTransactionOK() {
	return ErrorIdx;
}

uint8_t uart_tx(const char *cmd) {

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
#ifdef _DEBUG
		lcd_clear();
#endif
		lcd_print_progress((char *) cmd, LINE1);
	}

	return uart_tx_timeout(cmd, g_iModemMaxWait, 10);
}

int uart_rx(int atCMD, char* pResponse) {
	return uart_rx_cleanBuf(atCMD, pResponse, 0);
}

// Clears the response buffer if len > 0
int uart_rx_cleanBuf(int atCMD, char* pResponse, uint16_t reponseLen) {
	char* pToken1 = NULL;
	char* pToken2 = NULL;
	int bytestoread = 0;
	int iStartIdx = 0;
	int iEndIdx = 0;
	uint16_t mcc = 0;
	uint16_t mnc = 0;

	iRXCommandProcessed = 0;
	if (reponseLen > 0)
		memset(pResponse, 0, reponseLen);

	if (RXHeadIdx < RXTailIdx) {
		pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "CMS ERROR:");
		if (pToken1 != NULL) {
			// ERROR FOUND;
			delay(2000);
			lcd_clear();
			lcd_print_progress((char *) &RXBuffer[RXHeadIdx + 7], LINE1);
			delay(5000);
			return ret;
		} else {
#if defined(_DEBUG)
			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], ": ");
			if (pToken1!=NULL) {
				lcd_print_progress((char *) pToken1+2, LINE2);
			} else {
				lcd_print_progress((char *) (const char *) &RXBuffer[RXHeadIdx], LINE2);
			}
#endif
		}
	}

	//input check
	if (pResponse) {
		switch (atCMD) {
		case ATCMD_CSURVC:
			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "ERROR");
			if (pToken1 != NULL)
				return UART_ERROR;

			pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], "mcc:");
			if (pToken1!=NULL) mcc=atoi(pToken1+5);
				else mcc=0;

			pToken1 = strstr((const char *) pToken1, "mnc:");
			if (pToken1!=NULL) mnc=atoi(pToken1+5);
				else mnc=0;

			if (mcc!=0 && mnc!=0) {
				sprintf(ATresponse, "%d,%d", mcc, mnc);
				return UART_SUCCESS;
			}
			return UART_FAILED;

		case ATCMD_CMGS:
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

		case ATCMD_HTTPQRY:
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
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, (const char *) &RXBuffer[iStartIdx], bytestoread);
							ret = 0;
						} else {
							//rollover
							bytestoread = iRxLen - iStartIdx;
							if (bytestoread > CMGL_RSP_LEN) {
								bytestoread = CMGL_RSP_LEN;
							}
							memcpy(pResponse, (const char *)  &RXBuffer[iStartIdx], bytestoread);
							memcpy(&pResponse[bytestoread], (const char *) RXBuffer, iEndIdx);
							ret = 0;
						}
					}
				}
			}
			break;
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

void uart_setPromptMode() {
	uart_setCheckMsg(AT_MSG_PROMPT, AT_CME_ERROR);
}

void uart_setOKMode() {
	uart_setCheckMsg(AT_MSG_OK, AT_CME_ERROR);
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
		if (RXTailIdx >= sizeof(RXBuffer)) {
			RXTailIdx = 0;
			if (uart_numDataPages>0) {
				uart_numDataPages--;
				if (uart_numDataPages==0) {
					g_iUartState=UART_SUCCESS;
				}
			}
		}

		uart_checkOK();
		uart_checkERROR();

		if (uart_numDataPages!=0) {  // -1 or >1 will be capturing data
			RXBuffer[RXTailIdx++] = UCA0RXBUF;
			g_iRxCountBytes++;  // Data transfer check
		}

		if (UCA0STATW & UCRXERR) {
			iError++;
		}

		if (UCA0RXBUF == XOFF) {
			iTxStop = 1;
		} else if (UCA0RXBUF == XON) {
			iTxStop = 0;
		}

		if (TransmissionEnd)
			__bic_SR_register_on_exit(LPM0_bits); // Resume execution

		break;

	case USCI_SPI_UCTXIFG:
		UCA0TXBUF = TXBuffer[TXIdx];               // Transmit characters
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

void uart_setClock() {
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

