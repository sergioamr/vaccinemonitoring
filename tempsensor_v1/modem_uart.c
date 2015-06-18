/*
 * uart.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#include "msp430.h"
#include "config.h"
#include "common.h"
#include "modem_uart.h"
#include "stdlib.h"
#include "string.h"
#include "lcd.h"
#include "globals.h"
#include "stdio.h"
#include "string.h"
#include "stringutils.h"
#include "modem.h"
#include "fatdata.h"
#include "timer.h"

// AT Messages returns to check.
const char AT_MSG_OK[]={ 0x0D, 0x0A, 'O', 'K', 0x0D, 0x0A, 0 };
const char AT_MSG_PROMPT[]={ 0x0D, 0x0A, '>', 0};	// Used for SMS message sending
const char AT_MSG_LONG_PROMPT[]={ 0x0D, 0x0A, '>','>','>', 0};  // Used for http post transactions
const char AT_CME_ERROR[]= "+CME ERROR:";
const char AT_CMS_ERROR[]= "+CMS ERROR:"; // Sim error
const char AT_ERROR[]= " ERROR:";

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile char RXBuffer[RX_LEN];
char TXBuffer[TX_LEN] = "";
#pragma SET_DATA_SECTION()

volatile static char TransmissionEnd = 0;

volatile int RXTailIdx = 0;
volatile int RXHeadIdx = 0;
volatile int g_iWaitForTXEnd = 0;

volatile int8_t g_iUartState = 0;
volatile int g_iRxCountBytes = 0;
//volatile char* TX;
volatile int TXIdx = 0;
volatile int8_t iTxStop = 0;
volatile uint8_t iError = 0;
volatile uint8_t iTXInProgress = 0;
volatile uint8_t iRXCommandProcessed = 0; // A finished AT command arrived.

size_t iTxLen = 0;
size_t iRxLen = RX_LEN;
#define INDEX_2 2
#define INDEX_5 5
#define INDEX_7 7
#define INDEX_9 9
#define INDEX_12 12

// Carriage return
#define ATCR 10

void uart_resetbuffer() {
	RXHeadIdx = RXTailIdx = 0; //ZZZZ reset Rx index to faciliate processing in uart_rx
#ifdef _DEBUG_EXT
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

	if (g_iUartState==UART_ERROR) {
		if (UCA0RXBUF=='\n')
			TransmissionEnd=1;
		return;
	}

	if (UCA0RXBUF!=g_szError[++ErrorIdx]) {
		ErrorIdx=0;
		return;
	}

	if (ErrorIdx==ErrorLength) {
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

void uart_setupIO() {
	// P2.0 UCA0SIMO
	// P2.1 UCA0RXD/ UCA0SOMI

	P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
	P2SEL0 &= ~(BIT0 | BIT1);
	P4DIR |= BIT0 | BIT5 | BIT6 | BIT7; // Set P4.0 (Modem reset), LEDs to output direction

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

int8_t uart_getTransactionState() {
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
		lcd_print("TXBUFFER ERROR");
		delay(HUMAN_DISPLAY_ERROR_DELAY);
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

#define DEFAULT_INTERVAL_DELAY 2
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

	if (delayTime<=100) {
		delayTime=100;
		count=1;
	}

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

char modem_lastCommand[16];

// Try a command until success with timeout and number of attempts to be made at running it
uint8_t uart_tx_timeout(const char *cmd, uint32_t timeout, uint8_t attempts) {

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_print_progress();
	}
	strncpy(modem_lastCommand, cmd, 16);

	while(attempts>0) {
		sendCommand(cmd);
		if (!waitForReady(timeout)) {

			// Transaction could be sucessful but the modem could have return an error.
			modem_check_uart_error();
			uart_setRingBuffer();
			return UART_SUCCESS;
		}
		attempts--;
		if (g_iLCDVerbose == VERBOSE_BOOTING) {
			lcd_printl(LINEC, modem_lastCommand);
			lcd_print_boot("MODEM TIMEOUT", LINE2);
			log_appendf("TIMEOUT: SIM %d cmd[%s]", config_getSelectedSIM(), cmd);
		}
	}

	// If there was an error we have to wait a bit to retrieve everything that is left from the transaction, like the error message
	modem_check_uart_error();

	iModemErrors++;
	uart_setRingBuffer(); // Restore the ring buffer if it was changed.
	return UART_FAILED;
}

void uart_tx_nowait(const char *cmd) {
	g_iWaitForTXEnd = 1;
	sendCommand(cmd);
	while(g_iWaitForTXEnd == 1) {
		delay(5000);
	}
}

uint8_t uart_tx_waitForPrompt(const char *cmd, uint32_t promptTime) {
	sendCommand(cmd);
	if (!waitForReady(promptTime)) {
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

uint8_t uart_txf(const char *_format, ...) {
	char szTemp[128];
    va_list _ap;
    char *fptr = (char *)_format;
    char *out_end = szTemp;

    va_start(_ap, _format);
    if (__TI_printfi(&fptr, _ap, (void *)&out_end, _outc, _outs)>128) {
    	_NOP();
    }
    va_end(_ap);

    *out_end = '\0';
    return uart_tx( szTemp);
}

uint8_t uart_tx(const char *cmd) {
#ifdef _DEBUG_OUTPUT
	char* pToken1;
#endif
	int uart_state;
	int transaction_completed;

	uart_resetbuffer();

	transaction_completed = uart_tx_timeout(cmd, g_iModemMaxWait, 10);
	if (RXHeadIdx > RXTailIdx)
		return transaction_completed;

	uart_state = uart_getTransactionState();
	if (transaction_completed == UART_SUCCESS && uart_state != UART_ERROR) {
#ifdef _DEBUG_OUTPUT
		pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], ": \""); // Display the command returned
		if (pToken1 != NULL) {
			lcd_print_boot((char *) pToken1 + 3, LINE2);
		} else {
			lcd_print_progress((char *) (const char *) &RXBuffer[RXHeadIdx + 2], LINE2); // Display the OK message
		}
#endif
	} else
		modem_check_uart_error();

	return uart_state;
}

void uart_setHTTPPromptMode() {
	uart_setCheckMsg(AT_MSG_LONG_PROMPT, AT_ERROR);
}

void uart_setSMSPromptMode() {
	uart_setCheckMsg(AT_MSG_PROMPT, AT_CMS_ERROR);
}

void uart_setOKMode() {
	uart_setCheckMsg(AT_MSG_OK, AT_ERROR);
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
	switch (__even_in_range(UCA0IV, USCI_UART_UCTXIFG)) {
	case USCI_NONE:
		break;

	case USCI_UART_UCRXIFG:
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

	case USCI_UART_UCTXIFG:
		UCA0TXBUF = TXBuffer[TXIdx];               // Transmit characters
		TXIdx++;
		if (TXIdx >= iTxLen) {
			TXIdx = 0;
			UCA0IE &= ~UCTXIE; 	// Finished transmision
			if (g_iWaitForTXEnd == 1) {
				g_iWaitForTXEnd = 0;
				__bic_SR_register_on_exit(LPM0_bits);
			}
		}

		break;

	default:
		break;
	}
}

void uart_setupIO_clock() {
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

