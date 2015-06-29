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
const char AT_MSG_OK[] = { 0x0D, 0x0A, 'O', 'K', 0x0D, 0x0A, 0 };
const char AT_MSG_PROMPT[] = { 0x0D, 0x0A, '>', 0 }; // Used for SMS message sending
const char AT_MSG_LONG_PROMPT[] = { 0x0D, 0x0A, '>', '>', '>', 0 }; // Used for http post transactions
const char AT_MSG_HTTPRING[] = "#HTTPRING: ";
const char AT_CME_ERROR[] = "+CME ERROR:";
const char AT_CMS_ERROR[] = "+CMS ERROR:"; // Sim error
const char AT_ERROR[] = " ERROR:";

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile char RXBuffer[RX_LEN];
char TXBuffer[TX_LEN] = "";
#pragma SET_DATA_SECTION()

volatile int uart_numDataPages = -1; // Number of pages of data to retrieve. -1 is circular buffer
volatile UART_TRANSFER uart;

// Carriage return
#define ATCR 10

/*************************** ERROR AND OK  *****************************/
// Function to check end of msg
void uart_setCheckMsg(const char *szOK, const char *szError) {
	uart.iActive = 1;
	uart.bTransmissionEnd = 0;
	uart.iUartState = UART_INPROCESS;

	if (szError != NULL) {
		uart.ErrorIdx = 0;
		uart.ErrorLength = strlen(szError) - 1; // We don't check for 0 terminated string
		strncpy((char *) uart.szError, szError, sizeof(uart.szError));
	} else
		uart.ErrorLength = -1;

	if (szOK != NULL) {
		uart.OKIdx = 0;
		uart.OKLength = strlen(szOK) - 1; // We don't check for 0 terminated string
		strncpy((char *) uart.szOK, szOK, sizeof(uart.szOK));
	} else
		uart.OKLength = -1;

	// Wait for a return after OK
	uart.bRXWaitForReturn = 0;
}

// We define the exit condition for the wait for ready function
inline void uart_checkERROR() {

	if (uart.ErrorLength == -1)
		return;

	if (uart.iUartState == UART_ERROR) {
		if (UCA0RXBUF == '\n')
			uart.bTransmissionEnd = 1;
		return;
	}

	if (UCA0RXBUF != uart.szError[++uart.ErrorIdx]) {
		uart.ErrorIdx = 0;
		return;
	}

	if (uart.ErrorIdx == uart.ErrorLength) {
		uart.iUartState = UART_ERROR;
		uart.ErrorIdx = 0;
	}
}

// Cancels data transaction after a number of pages has been obtained
void uart_setNumberOfPages(int numPages) {
	uart_numDataPages = numPages;
}

void uart_setRingBuffer() {
	uart_numDataPages = -1;
}

// We define the exit condition for the wait for ready function
inline void uart_checkOK() {

	if (uart.OKLength == -1)
		return;

	if (uart.bRXWaitForReturn && uart.iUartState == UART_SUCCESS) {
		if (UCA0RXBUF == 0x0A)
			uart.bTransmissionEnd = 1;
		return;
	}

	if (UCA0RXBUF != uart.szOK[++uart.OKIdx]) {
		uart.OKIdx = 0;
		return;
	}

	// Wait for carriage return
	if (uart.OKIdx == uart.OKLength) {
		if (!uart.bRXWaitForReturn)
			uart.bTransmissionEnd = 1;
		else
			_NOP();
		uart.iUartState = UART_SUCCESS;
		uart.OKIdx = 0;
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
	uart.iActive = 1;
}

int8_t uart_getTransactionState() {
	SIM_CARD_CONFIG *sim = config_getSIM();

	if (!sim->simOperational)
		return UART_ERROR;

	return uart.iUartState;
}

const char *uart_getRXHead() {
	if (uart.iRXHeadIdx >= sizeof(RXBuffer)) {
		uart.iRXHeadIdx = 0;
		_NOP();
	}
	return (const char *) &RXBuffer[uart.iRXHeadIdx];
}

// Reset all buffers, headers, and counters for transmission
void uart_reset_headers() {
#ifdef _DEBUG
	memset((char *) RXBuffer, 0, sizeof(RXBuffer));
#else
	RXBuffer[0]=0;
#endif

	uart.iTXIdx = 0;
	uart.iRXHeadIdx = uart.iRXTailIdx = 0;
	uart.bTransmissionEnd = 0;

	uart.OKIdx = 0;     // Indexes for checking for end of command
	uart.ErrorIdx = 0;
	uart.iUartState = UART_INPROCESS; // Setup in process, we are actually going to generate a command

	uart.iRxCountBytes = 0; // Stats to know how much data are we getting on this buffer.
}

void modem_send_command(const char *cmd) {

	// Clear reset
	uart_reset_headers();

	// Only copy the buffer if we are not actually using TXBuffer,
	// some places of the old code use the TXBuffer directly :-/

	uart.iTxLen = strlen(cmd);

	if (g_pDevCfg->cfg.logs.modem_transactions) {
		log_modem("-------- ");
		log_modem(get_simplified_date_string(NULL));
		log_modem("-------- \r\n");
		log_modem(cmd);
	}

	// Store the maximum size used from this buffer
	if (uart.iTxLen > g_pSysCfg->maxTXBuffer)
		g_pSysCfg->maxTXBuffer = uart.iTxLen;

	if (uart.iTxLen > sizeof(TXBuffer)) {
		lcd_print("TXBUFFER ERROR");
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	}

	if (cmd != TXBuffer) {
#ifdef _DEBUG
		memset((char *) TXBuffer, 0, sizeof(TXBuffer));
#else
		TXBuffer[0]=0;
#endif
		memcpy((char *) TXBuffer, cmd, uart.iTxLen);
	}

	UCA0IE |= UCRXIE;   // Enable USCI_A0 RX interrupt
	UCA0IE |= UCTXIE;

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
	uint32_t delayTime = timeoutTimeMs / delayDivider;
	int count = delayDivider;

	if (delayTime <= 100) {
		delayTime = 100;
		count = 1;
	}

	while (count > 0) {
		delay(delayTime);
		if (uart.bTransmissionEnd == 1) {
			delay(100);  // Documentation specifies 30 ms delay between commands
			if (g_pDevCfg->cfg.logs.modem_transactions)
				log_modem(uart_getRXHead());

			return UART_SUCCESS; // There was a transaction, you have to check the state of the uart transaction to check if it was successful
		}
		count--;
	}

	if (g_pDevCfg->cfg.logs.modem_transactions) {
		log_modem("FAILED\r\n");
		log_modem(uart_getRXHead());
	}

	// Store the maximum size used from this buffer
	if (uart.iRxCountBytes > g_pSysCfg->maxRXBuffer)
		g_pSysCfg->maxRXBuffer = uart.iRxCountBytes;

	delay(100);  // Documentation specifies 30 ms delay between commands

	if (uart.iUartState != UART_SUCCESS) {
		uart.iUartState = UART_TIMEOUT;
		return UART_FAILED;
	}

	return UART_SUCCESS;
}

uint8_t iModemErrors = 0;

char modem_lastCommand[16];

// Try a command until success with timeout and number of attempts to be made at running it
uint8_t uart_tx_timeout(const char *cmdInput, uint32_t timeout,
		uint8_t attempts) {
	char *cmd = modem_lastCommand;

	if (!config_isSimOperational()) {
		return UART_FAILED;
	}

	int len = strlen(cmdInput);
	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_print_progress();
	}

	zeroTerminateCopy(modem_lastCommand, cmdInput);
	if (len < 16) {
		if (cmd[len - 1] != '\n') {
			strcat(cmd, "\r\n");
			_NOP();
		} else {
			_NOP();
		}
	} else
		cmd = (char *) cmdInput;

	while (attempts > 0) {
		modem_send_command(cmd);
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
		}
	}

	// If there was an error we have to wait a bit to retrieve everything that is left from the transaction, like the error message
	modem_check_uart_error();

	iModemErrors++;
	uart_setRingBuffer(); // Restore the ring buffer if it was changed.
	return UART_FAILED;
}

void uart_tx_nowait(const char *cmd) {
	uart.bWaitForTXEnd = 1;
	modem_send_command(cmd);
	while (uart.bWaitForTXEnd == 1) {
		delay(5000);
	}
}

uint8_t uart_tx_waitForPrompt(const char *cmd, uint32_t promptTime) {
	modem_send_command(cmd);
	if (!waitForReady(promptTime)) {
		uart_setOKMode();
		return 1; // We found a prompt
	}

	uart_setOKMode();
	return 0;
}

uint32_t g_iModemMaxWait = MODEM_TX_DELAY1;

void uart_setDelay(uint32_t delay) {
	g_iModemMaxWait = MODEM_TX_DELAY1;
}

uint8_t isTransactionOK() {
	return uart.ErrorIdx;
}

uint8_t uart_txf(const char *_format, ...) {
	char szTemp[128];
	va_list _ap;
	char *fptr = (char *) _format;
	char *out_end = szTemp;

	va_start(_ap, _format);
	if (__TI_printfi(&fptr, _ap, (void *) &out_end, _outc, _outs) > 128) {
		_NOP();
	}
	va_end(_ap);

	*out_end = '\0';
	return uart_tx(szTemp);
}

uint8_t uart_tx(const char *cmd) {
#ifdef _DEBUG_OUTPUT
	char* pToken1;
#endif
	int uart_state;
	uart_reset_headers();

	uart_tx_timeout(cmd, g_iModemMaxWait, 10);
	uart_state = uart_getTransactionState();
#ifdef _DEBUG_OUTPUT
	if (transaction_completed == UART_SUCCESS && uart_state != UART_ERROR) {
		pToken1 = strstr((const char *) &RXBuffer[RXHeadIdx], ": \""); // Display the command returned
		if (pToken1 != NULL) {
			lcd_print_boot((char *) pToken1 + 3, LINE2);
		} else {
			lcd_print_progress((char *) (const char *) &RXBuffer[RXHeadIdx + 2], LINE2); // Display the OK message
		}
	}
#endif
	return uart_state;
}

void uart_setHTTPPromptMode() {
	uart_setCheckMsg(AT_MSG_LONG_PROMPT, AT_ERROR);
	uart.bRXWaitForReturn = 0;
}

void uart_setHTTPDataMode() {
	uart_setCheckMsg(AT_MSG_HTTPRING, AT_ERROR);
	uart.bRXWaitForReturn = 1;
}

void uart_setSMSPromptMode() {
	uart_setCheckMsg(AT_MSG_PROMPT, AT_CMS_ERROR);
	uart.bRXWaitForReturn = 0;
}

void uart_setOKMode() {
	uart_setCheckMsg(AT_MSG_OK, AT_ERROR);
	uart.bRXWaitForReturn = 0;
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
		if (!uart.iActive)
			return;

		if (uart.iRXTailIdx >= sizeof(RXBuffer)) {
			uart.iRXTailIdx = 0;
			if (uart_numDataPages > 0) {
				uart_numDataPages--;
				if (uart_numDataPages == 0) {
					uart.iUartState = UART_SUCCESS;
				}
			}
		}

		uart_checkOK();
		uart_checkERROR();

		if (uart_numDataPages != 0) {  // -1 or >1 will be capturing data
			RXBuffer[uart.iRXTailIdx++] = UCA0RXBUF;
			uart.iRxCountBytes++;  // Data transfer check
		}

		if (UCA0STATW & UCRXERR) {
			uart.iError++;
		}

		if (uart.bTransmissionEnd) {
			RXBuffer[uart.iRXTailIdx] = 0;
			__bic_SR_register_on_exit(LPM0_bits); // Resume execution
		}

		break;

	case USCI_UART_UCTXIFG:
		if (!uart.iActive)
			return;

		UCA0TXBUF = TXBuffer[uart.iTXIdx];               // Transmit characters
		uart.iTXIdx++;
		if (uart.iTXIdx >= uart.iTxLen) {
			uart.iTXIdx = 0;
			UCA0IE &= ~UCTXIE; 	// Finished transmision
			if (uart.bWaitForTXEnd == 1) {
				uart.bWaitForTXEnd = 0;
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

	//UCA0IE |= UCRXIE;                // Enable USCI_A0 RX interrupt
}

