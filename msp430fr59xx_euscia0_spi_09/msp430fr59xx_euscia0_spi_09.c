/* --COPYRIGHT--,BSD_EX
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************
 *
 *                       MSP430 CODE EXAMPLE DISCLAIMER
 *
 * MSP430 code examples are self-contained low-level programs that typically
 * demonstrate a single peripheral function or device feature in a highly
 * concise manner. For this the code may rely on the device's power-on default
 * register values and settings such as the clock configuration and care must
 * be taken when combining code from several examples to avoid potential side
 * effects. Also see www.ti.com/grace for a GUI- and www.ti.com/msp430ware
 * for an API functional library-approach to peripheral configuration.
 *
 * --/COPYRIGHT--*/
//******************************************************************************
//   MSP430F59xx Demo - eUSCI_A0, SPI 3-Wire Master Incremented Data
//
//   Description: SPI master talks to SPI slave using 3-wire mode. Incrementing
//   data is sent by the master starting at 0x01. Received data is expected to
//   be same as the previous transmission TXData = RXData-1.
//   USCI RX ISR is used to handle communication with the CPU, normally in LPM0.
//   ACLK = 32.768kHz, MCLK = SMCLK = DCO ~1MHz.  BRCLK = ACLK/2
//
//
//                   MSP430FR5969
//                 -----------------
//            /|\ |              XIN|-
//             |  |                 |  32KHz Crystal
//             ---|RST          XOUT|-
//                |                 |
//                |             P2.0|-> Data Out (UCA0SIMO)
//                |                 |
//                |             P2.1|<- Data In (UCA0SOMI)
//                |                 |
//                |             P1.5|-> Serial Clock Out (UCA0CLK)
//
//   P. Thanigai
//   Texas Instruments Inc.
//   Feb 2012
//   Built with CCS V5.5
//******************************************************************************
#define MAX_NUM_SENSORS		5

#include "stdint.h"
#include <msp430.h>
#include "i2c.h"
#include "timer.h"
#include "stdlib.h"
#include "uart.h"
#include "config.h"
#include "framctl.h"
#include "battery.h"
#include "util.h"
#include "common.h"
#include "driverlib.h"

#define  CALIBRATE 4	//0 - default, 1 = device A, 2 = device B...
#define SMS_TEST
#define LCD_TEST
#define ENABLE_SIM_SLOT		//needed to set on new board, comment it for old board
volatile uint8_t iStatus = 1;
extern volatile uint32_t iMinuteTick;

//len constants
#define IMEI_MAX_LEN		 15
#define APN_MAX_LEN     	 20
#define UPLOAD_MODE_LEN 	 4
#define MAX_SMS_NUM			 4
#define SMS_NUM_LEN			 12
#define SMS_READ_MAX_MSG_IDX 25		//ZZZZ to change date based on CPMS

#define SMS_ENCODED_LEN		 154	//ZZZZ SMS_ENCODED_LEN + ENCODED_TEMP_LEN should less than aggregate_var size - RX buff size
#define ENCODED_TEMP_LEN	 3

#define SMS_HB_MSG_TYPE  	 "10,"
#define SMS_DATA_MSG_TYPE	 "11,"
//INFO memory segment address
#define INFOA_ADDR      0x1980
#define INFOB_ADDR      0x1900

//iStatus contants
#define ENABLE_SLOT1	0x0010
#define RESET_ALERT  	0x0020

#define RX_EXTENDED_LEN 300		//to retreive a complete http get cfg message

#define MODEM_POWERED_ON 	0x0001
#define MODEM_CHECK_RETRY 	3
#define MODEM_TX_DELAY1		1000
#define MODEM_TX_DELAY2		5000

#define SPI_READID_CMD   		0x90
#define SPI_PAGEPROGRAM_CMD		0x02
#define SPI_READDATA_CMD		0x03
#define SPI_ERASE4K_CMD			0x20
#define SPI_WREN_CMD			0x06

#define PAGE_SIZE				256

#define SPI_TX_6(CMD, DATA1, DATA2, DATA3, DATA4, DATA5) \
		{				 	\
			TX[0] = CMD; 	\
			TX[1] = DATA1; 	\
			TX[2] = DATA2; 	\
			TX[3] = DATA3; 	\
			TX[4] = DATA4;	\
			TX[5] = DATA5; 	\
			iTxLen = 6;		\
		}

#define SPI_TX_5(CMD, DATA1, DATA2, DATA3, DATA4) \
		{				 	\
			TX[0] = CMD; 	\
			TX[1] = DATA1; 	\
			TX[2] = DATA2; 	\
			TX[3] = DATA3; 	\
			TX[4] = DATA4;	\
			iTxLen = 5;		\
		}

#define SPI_TX_4(CMD, DATA1, DATA2, DATA3) \
		{				 	\
			TX[0] = CMD; 	\
			TX[1] = DATA1; 	\
			TX[2] = DATA2; 	\
			TX[3] = DATA3; 	\
			iTxLen = 4;		\
		}

#define SPI_TX_1(CMD) \
		{				 	\
			TX[0] = CMD; 	\
			iTxLen = 1;		\
		}

#define SPI_TX_PROGRAM(ADDR, PDATA, LEN)				\
		{					\
			TX[0] = SPI_PAGEPROGRAM_CMD; 	\
			TX[1] = (ADDR >> 16) & 0xFF; 	\
			TX[2] = (ADDR >> 8) & 0xFF; 	\
			TX[3] = ADDR & 0xFF; 			\
			memcpy(&TX[4],PDATA,LEN);		\
			iTxLen = 4 + LEN;				\
		}

#include <msp430.h>

#ifdef SPI_NOR_TEST
volatile char RX[RX_LEN];
volatile int RXTailIdx = 0;
volatile int RXHeadIdx = 0;
volatile char TX[TX_LEN] = {};
//volatile char TX[TX_LEN] = {0x94, 0x00, 0x00, 0x00};
volatile int TXIdx = 0;
volatile int iTXInProgress = 0;
volatile int iError = 0;
int iTxLen = 0;
#endif

char TMP[25];
char SampleData[400];
static char str[25];
int32_t timedate;

char ATresponse[158];
char ScratchPad[256];		//use SampleData during final integration

CONFIG_INFOB* g_pInfoB = (CONFIG_INFOB*) ScratchPad;
CONFIG_INFOA* pstCfgInfoA = (CONFIG_INFOA*) INFOA_ADDR;
CONFIG_INFOB* pstCfgInfoB = (CONFIG_INFOB*) INFOB_ADDR;
#ifdef LCD_TEST
void lcd_tx(int8_t TXdata) {
	//wait for TX to be empty
	while (!(UCB0IFG & UCTXIFG0))
		;
	//write to TX buffer
	UCB0TXBUF = TXdata;
}

void lcd_reset() {
	PJOUT &= ~BIT6;
	delay(100);	//delay 100 ms
	PJOUT |= BIT6;
}

void lcd_blenable() {
	PJOUT |= BIT7;
}

void lcd_bldisable() {
	PJOUT &= ~BIT7;
}
#endif
extern char __STACK_END; /* the type does not matter! */
extern char __STACK_SIZE; /* the type does not matter! */
volatile int8_t iSlot = 1;

int main(void) {
	int iVal = 1000;
	int iIdx = 0;
	int iPOSTstatus = 0;
	double temp = 0.0;
	char* pcTmp = NULL;
	char* pcData = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	uint32_t iSampleTimeElapsed = 0;
	uint32_t iAlarmStatus = 0;
	int8_t iLastSlot = -1;

	WDTCTL = WDTPW | WDTHOLD;                 // Stop watchdog timer

	iVal = &__STACK_SIZE;
	memset((void*) (&__STACK_END - &__STACK_SIZE), 0xa5, iVal / 4); //paint 1/4th stack with know values

	// Configure GPIO
	P2DIR |= BIT3;							// SPI CS
	P2OUT |= BIT3;					// drive SPI CS high to deactive the chip
	P2SEL1 |= BIT4 | BIT5 | BIT6;             // enable SPI CLK, SIMO, SOMI
	PJSEL0 |= BIT4 | BIT5;                    // For XT1
	P1SEL1 |= BIT6 | BIT7;					// Enable I2C SDA and CLK

	P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
	P2SEL0 &= ~(BIT0 | BIT1);
	P4DIR |= BIT0 | BIT5 | BIT6 | BIT7; // Set P4.0 (Modem reset), LEDs to output direction
	P4OUT &= ~BIT0;                           // Reset high

	P3DIR |= BIT4;      						// Set P3.4 buzzer output

	PJDIR |= BIT6 | BIT7;      			// set LCD reset and Backlight enable
	PJOUT |= BIT6;							// LCD reset pulled high
	PJOUT &= ~BIT7;							// Backlight disable
	P2IE |= BIT2;							// enable interrupt for buzzer off

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	// Startup clock system with max DCO setting ~8MHz
	CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
	CSCTL1 = DCOFSEL_3 | DCORSEL;             // Set DCO to 8MHz
	CSCTL2 = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
	//CSCTL3 = DIVA__1 | DIVS__8 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers TODO - divider
	CSCTL4 &= ~LFXTOFF;
	do {
		CSCTL5 &= ~LFXTOFFG;                    // Clear XT1 fault flag
		SFRIFG1 &= ~OFIFG;
	} while (SFRIFG1 & OFIFG);                   // Test oscillator fault flag
	CSCTL0_H = 0;                             // Lock CS registers

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

#ifdef SPI_NOR_TEST
	// Configure USCI_A0 for SPI operation
	UCA1CTLW0 = UCSWRST;// **Put state machine in reset**
	//UCA1CTLW0 |= UCMST | UCSYNC | UCCKPH | UCMSB; // 3-pin, 8-bit SPI master, clock polarity=0, clock phase=1 (captured on first edge and sampled in next)
	// MSB
	UCA1CTLW0 |= UCMST | UCSYNC | UCCKPL | UCMSB;// 3-pin, 8-bit SPI master, clock polarity=0, clock phase=1 (changed on first edge and captured in next)
	// MSB
	UCA1CTLW0 |= UCSSEL__SMCLK;// SMCLK - 1MHz
	UCA1BR0 = 0x00;//  SPI clk - 1MHz
	UCA1BR1 = 0;//
	UCA1MCTLW = 0;// No modulation
	UCA1CTLW0 &= ~UCSWRST;// **Initialize USCI state machine**

	//iTxLen = 6;
	UCA1IE |= UCRXIE | UCTXIE;;// Enable USCI_A0 RX, TX interrupt
#endif

	__bis_SR_register(GIE);

	timedate = 86400;
	MPY32CTL0 |= OP2_32;
	MPYS = 13;
	OP2L = timedate & 0xFFFF;
	OP2H = timedate >> 16;
	timedate = RESHI;
	timedate = timedate << 16;
	timedate = timedate | RESLO;
	timedate = timedate / 3600;

	temp = strtod("23", NULL);
	if (temp < 25.0) {
		iVal += 500;
	} else {
		iVal -= 500;
	}
	temp = strtod("-23.7", NULL);
	if (temp < 25.0) {
		iVal += 500;
	} else {
		iVal -= 500;
	}

#if 0
	iIdx = 50;
	while(iIdx > 0)
	{
		P3OUT |= BIT4;
		delayus(1);
		P3OUT &= ~BIT4;
		delayus(1);
	}
#endif

#ifdef SEG_TEST
	i2c_init(400000);
	writetoI2C (0x01, 0x0f);						// Decode Mode register

	writetoI2C (0x02, 0x0B);//Intensity register

	writetoI2C (0x04, 0x01);//Configuration Register

	writetoI2C(0x20, 0x0A);//Display channel

	writetoI2C(0x21, 4);

	writetoI2C(0x22, 2);

	writetoI2C(0x23, 0);

	writetoI2C (0x24, 0x04);
#endif

	memset(TMP, 0, sizeof(TMP));
	strcat(TMP, "/W");	//add encoded temp value
	memset(ScratchPad, 0, sizeof(ScratchPad));
	memcpy(ScratchPad, INFOB_ADDR, sizeof(CONFIG_INFOB));

#if CALIBRATE == 1
	//device A, sensor A
	g_pInfoB->calibration[0][0]= -0.0089997215012;
	g_pInfoB->calibration[0][1]= 1.0168558678;

	//device A, sensor B
	g_pInfoB->calibration[1][0]= -0.1246539991;
	g_pInfoB->calibration[1][1]= 1.0160181876;

	//device A, sensor C
	g_pInfoB->calibration[2][0]= 0.0113285399;
	g_pInfoB->calibration[2][1]= 1.0181842794;

	//device A, sensor D
	g_pInfoB->calibration[3][0]= -0.2116162843;
	g_pInfoB->calibration[3][1]= 1.0142320871;

	//device A, sensor E
	g_pInfoB->calibration[4][0]= -0.1707975911;
	g_pInfoB->calibration[4][1]= 1.018139702;

#elif CALIBRATE==2
	//device B, sensor A
	g_pInfoB->calibration[0][0]= -0.11755709;
	g_pInfoB->calibration[0][1]= 1.0126639507;

	//device B, sensor B
	g_pInfoB->calibration[1][0]= -0.1325491351;
	g_pInfoB->calibration[1][1]= 1.0130573199;

	//device B, sensor C
	g_pInfoB->calibration[2][0]= 0.0171280734;
	g_pInfoB->calibration[2][1]= 1.0133498078;

	//device B, sensor D
	g_pInfoB->calibration[3][0]= -0.180533434;
	g_pInfoB->calibration[3][1]= 1.0138440268;

	//device B, sensor E
	g_pInfoB->calibration[4][0]= -0.0898083156;
	g_pInfoB->calibration[4][1]= 1.017814462;

#elif CALIBRATE==3
	//device C, sensor A
	g_pInfoB->calibration[0][0]= -0.2848321;
	g_pInfoB->calibration[0][1]= 1.021466371;

	//device C, sensor B
	g_pInfoB->calibration[1][0]= -0.289122994;
	g_pInfoB->calibration[1][1]= 1.022946138;

	//device C, sensor C
	g_pInfoB->calibration[2][0]= -0.208889079;
	g_pInfoB->calibration[2][1]= 1.02581601;

	//device C, sensor D
	g_pInfoB->calibration[3][0]= -0.28968586;
	g_pInfoB->calibration[3][1]= 1.023883188;

	//device C, sensor E
	g_pInfoB->calibration[4][0]= -0.0765155;
	g_pInfoB->calibration[4][1]= 1.021308586;

#elif CALIBRATE==4
	//device D, sensor A
	g_pInfoB->calibration[0][0] = 0.15588;
	g_pInfoB->calibration[0][1] = 1.013487;

	//device D, sensor B
	g_pInfoB->calibration[1][0] = -0.09636;
	g_pInfoB->calibration[1][1] = 1.011564;

	//device D, sensor C
	g_pInfoB->calibration[2][0] = -0.07391;
	g_pInfoB->calibration[2][1] = 1.010255;

	//device D, sensor D
	g_pInfoB->calibration[3][0] = 0.190958;
	g_pInfoB->calibration[3][1] = 1.017441;

	//device D, sensor E
	g_pInfoB->calibration[4][0] = 0.11215;
	g_pInfoB->calibration[4][1] = 1.010953;

#endif

	//memcpy(g_pInfoB,ScratchPad,sizeof(CONFIG_INFOB));
	FRAMCtl_write8(g_pInfoB, INFOB_ADDR, sizeof(CONFIG_INFOB));

#ifdef LCD_TEST
	//test the LCD
	memset(TMP, 0, sizeof(TMP));
	lcd_reset();
	lcd_blenable();
	i2c_init(400000);
	//i2c_init(380000);
	delay(1000);
#if 0
	i2c_setmode(I2C_POLL);
	UCB0I2CSA = 0x3e;
	UCB0CTL1 |= UCTR;
	UCB0CTL1 |= UCTXSTT;                    // I2C start condition

	lcd_tx(0);//control byte (instruction write, stream of bytes terminated with stop condition)
	lcd_tx(0x38);
	//delay(10);	//delay 10 ms
	lcd_tx(0x39);
	//delay(10);
	lcd_tx(0x14);
	lcd_tx(0x78);
	lcd_tx(0x5E);
	lcd_tx(0x6D);
	lcd_tx(0x0C);
	lcd_tx(0x01);
	lcd_tx(0x06);
	//delay(10);

	//trigger a stop condition
	UCB0CTL1 |= UCTXSTP;

	UCB0CTL1 |= UCTXSTT;// I2C start condition
	lcd_tx(0x40);//control byte (data write, stream of bytes terminated with stop condition)
	lcd_tx(0x41);//character A
	//delay(100);
	lcd_tx(0x42);//character A
	//delay(100);
	//trigger a stop condition
	UCB0CTL1 |= UCTXSTP;
#else
	TMP[0] = 0x38;
	TMP[1] = 0x39;
	TMP[2] = 0x14;
	TMP[3] = 0x78;
	TMP[4] = 0x5E;
	TMP[5] = 0x6D;
	TMP[6] = 0x0C;
	TMP[7] = 0x01;
	TMP[8] = 0x06;

	i2c_write(0x3e, 0, 9, TMP);
	delay(100);
	TMP[0] = 0x41;
	TMP[1] = 0x42;
	i2c_write(0x3e, 0x40, 2, TMP);
	delay(100);

	TMP[0] = 0x40 | 0x80;
	i2c_write(0x3e, 0, 1, TMP);
	delay(100);
	TMP[0] = 0x43;
	TMP[1] = 0x44;
	i2c_write(0x3e, 0x40, 2, TMP);
	delay(100);
#endif
#endif

#ifdef SMS_TEST
	{
		int8_t iStatus = 0;
		int iIdx = 0;

		//check Modem is powered on
		for (iIdx = 0; iIdx < MODEM_CHECK_RETRY; iIdx++) {
			if ((P4IN & BIT0) == 0) {
				iStatus |= MODEM_POWERED_ON;
				break;
			} else {
				iStatus &= ~MODEM_POWERED_ON;
				delay(100);
			}
		}

		while (1) {
			if (iLastSlot != iSlot) {
				iLastSlot = iSlot;

				if (iStatus & MODEM_POWERED_ON) {
#ifdef ENABLE_SIM_SLOT
					if (iSlot == 0) {
						//enable SIM A (slot 1)
						uart_tx("AT#GPIO=2,0,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#GPIO=4,1,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#GPIO=3,0,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#SIMDET=0\r\n");
						delay(5000);
						uart_tx("AT#SIMDET=1\r\n");
						delay(MODEM_TX_DELAY1);
					} else {
						//enable SIM B (slot 2)
						uart_tx("AT#GPIO=2,1,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#GPIO=4,0,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#GPIO=3,1,1\r\n");
						delay(MODEM_TX_DELAY1);
						uart_tx("AT#SIMDET=0\r\n");
						delay(5000);
						uart_tx("AT#SIMDET=1\r\n");
						delay(MODEM_TX_DELAY1);
					}
#endif

					uart_tx("AT+CMGF=1\r\n");	// set sms format to text mode
					delay(MODEM_TX_DELAY1);
					uart_tx("AT+CMEE=2\r\n");
					delay(MODEM_TX_DELAY1);
					uart_tx("AT#CMEEMODE=1\r\n");
					delay(MODEM_TX_DELAY1);
					uart_tx("AT&K4\r\n");
					delay(MODEM_TX_DELAY1);
					uart_tx("AT&P0\r\n");
					delay(MODEM_TX_DELAY1);
					uart_tx("AT&W0\r\n");
					delay(MODEM_TX_DELAY1);

					// uart_tx("AT+CSCA?\r\n");
					delay(MODEM_TX_DELAY1);

#if 1
					memset(ATresponse, 0, sizeof(ATresponse));
					strcat(ATresponse,
							"This is a test message.");
					iIdx = strlen(ATresponse);
					ATresponse[iIdx] = 0x1A;
					sendmsg(ATresponse);
					//sendmsgDA(ATresponse,"8151938952");
					delay(1000);
#endif

					// uart_tx("AT+CSDH=0\r\n");
					//delay(MODEM_TX_DELAY1);

					//uart_tx("AT+CSCA=\"919845087001\",145\r\n");
					//delay(MODEM_TX_DELAY1);
					//uart_tx("AT+CSCA?\r\n");
					//delay(MODEM_TX_DELAY1);

					//uart_tx("AT#GPIO=7,0,2\r\n");
					//delay(5000);
					uart_tx(
							"AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n");
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#SGACT=1,1\r\n");
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
					//uart_tx("AT#HTTPCFG=1,\"54.175.222.246\",80\r\n");

					delay(MODEM_TX_DELAY2);

					//reset the RX counters to reuse memory from POST DATA
					RXTailIdx = RXHeadIdx = 0;
					iRxLen = RX_EXTENDED_LEN;
					RX[RX_EXTENDED_LEN + 1] = 0;	//null termination

					memset(ATresponse, 0, sizeof(ATresponse));
					strcpy(ATresponse,
							"AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/358072043119046/1/\"\r\n");
					//strcpy(ScratchPad,"AT#HTTPQRY=1,0,\"/ip\"\r\n");

					uart_tx(ATresponse);
					delay(10000);
					uart_tx("AT#HTTPRCV=1\r\n");
					delay(10000);
					memset(ATresponse, 0, sizeof(ATresponse));
					uart_rx(ATCMD_HTTPQRY, ATresponse);
					delay(5000);

					//uart_tx("AT+CPMS=\"SM\"\r\n");
					//delay(5000);
					// uart_tx("AT+CNMI?\r\n");
					//delay(5000);
					// uart_tx("AT+CPMS?\r\n");
					// delay(5000);
					iSlot = !iSlot;

				}

			}
		}
	}

#endif

#if 0
	iTXInProgress = 1;
	P2OUT &= ~BIT3;
	UCA1IE |= UCTXIE;
	while(iTXInProgress != 0);
	P2OUT |= BIT3;

	iTxLen = 5;
	TXIdx = 1;
	iTXInProgress = 1;				//next message is ready
	P2OUT &= ~BIT3;
	UCA1TXBUF = 0x03;
	while(iTXInProgress != 0);
	P2OUT |= BIT3;

	iTxLen = 6;
	TXIdx = 1;
	iTXInProgress = 1;//next message is ready
	P2OUT &= ~BIT3;
	UCA1TXBUF = 0x4b;
	while(iTXInProgress != 0);
	P2OUT |= BIT3;
#endif

#if SPI_NOR_TEST
	spi_init();
	spi_write(0,"abcdefghiklmnopqr",17);
	spi_read(0,TMP,25);
#endif

	//delallmsg();
	//delay(5000);
	//sms config reception and processing
#if 1
	iVal = 13;
	while (iVal < SMS_READ_MAX_MSG_IDX) {
		memset(ATresponse, 0, sizeof(ATresponse));
		recvmsg(iVal, ATresponse);
		if (ATresponse[0] == '$')	//check for $
				{
			processmsg(ATresponse);
			memset(ATresponse, 0, sizeof(ATresponse));
			delmsg(iVal, ATresponse);
		}

		iVal++;
	}
#endif
	//sms encode
	memset(SampleData, 0, sizeof(SampleData));
	//358072043113601
	strcat(SampleData,
			"IMEI=358072043119046&ph=00447542972925&v=1.20140817.1&sid=0|1|2|3&sdt=2015/03/03:08:22:08&i=1&t=25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7&b=100,100,100,100,100,100,100,100,100,100&p=1,1,1,1,1,1,1,1,1,1");
	//strcat(SampleData,"IMEI=353173063204364&ph=8455523642&v=1.20140817.1&sid=0|1|2|3&sdt=2015/03/03:08:22:08&i=1&t=25.1,23.1|26.1,23.1|25.9,23.1|24.3,23.1&b=100,100&p=1,1");
	//strcat(SampleData,"IMEI=353173063204364&ph=8455523642&v=1.20140817.1&sid=0|1|2|3&sdt=2015/03/03:08:22:08&i=1&t=25.1|26.1|25.9|24.3&b=100&p=1");

	iPOSTstatus = strlen(SampleData);

	memset(ATresponse, 0, sizeof(ATresponse));
	strcat(ATresponse, SMS_DATA_MSG_TYPE);

	pcSrc1 = strstr(SampleData, "sdt=");	//start of TS
	pcSrc2 = strstr(pcSrc1, "&");	//end of TS
	pcTmp = strtok(&pcSrc1[4], "/:");

	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(ATresponse, pcTmp);	//get year
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, pcTmp);	//get month
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(ATresponse, pcTmp);	//get day
				strcat(ATresponse, ":");
			}
		}

		//fetch time
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, pcTmp);	//get hour
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(ATresponse, pcTmp);	//get minute
				pcTmp = strtok(NULL, "&");
				if ((pcTmp) && (pcTmp < pcSrc2)) {
					strcat(ATresponse, pcTmp);	//get sec
					strcat(ATresponse, ",");
				}
			}
		}
	}

	pcSrc1 = strstr(pcSrc2 + 1, "i=");	//start of interval
	pcSrc2 = strstr(pcSrc1, "&");	//end of interval
	pcTmp = strtok(&pcSrc1[2], "&");
	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(ATresponse, pcTmp);
		strcat(ATresponse, ",");
	}

	pcSrc1 = strstr(pcSrc2 + 1, "t=");	//start of temperature
	pcData = strstr(pcSrc1, "&");	//end of temperature
	pcSrc2 = strstr(pcSrc1, ",");
	if ((pcSrc2) && (pcSrc2 < pcData)) {
		iIdx = 0xA5A5;	//temperature has series values
	} else {
		iIdx = 0xDEAD;	//temperature has single value
	}

	//check if temperature values are in series
	if (iIdx == 0xDEAD) {
		strcat(ATresponse, "0,");
		//pcSrc1 is t=23.1|24.1|25.1|26.1
		strncat(ATresponse, &pcSrc1[2], 4); //get first sensor temp value
		strcat(ATresponse, ",1,");
		strncat(ATresponse, &pcSrc1[7], 4); //get second sensor temp value
		strcat(ATresponse, ",2,");
		strncat(ATresponse, &pcSrc1[12], 4); //get second sensor temp value
		strcat(ATresponse, ",3,");
		strncat(ATresponse, &pcSrc1[17], 4); //get second sensor temp value
		strcat(ATresponse, ",");
		pcTmp = strtok(pcSrc1, "&");  //past the temperature fields
		pcTmp = strtok(NULL, ",&");  //past the first battery value
	} else {
		iIdx = 0;
		pcSrc2 = strstr(pcSrc1, "|");	//end of first sensor
		if (!pcSrc2)
			pcSrc2 = pcData;
		pcTmp = strtok(&pcSrc1[2], ",|&");  //get first temp value

		while ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, itoa_nopadding(iIdx));	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
			encode(strtod(pcTmp, NULL), pcSrc1);
			strcat(ATresponse, pcSrc1);	//add encoded temp value

			pcTmp = strtok(NULL, ",|&");
			while ((pcTmp) && (pcTmp < pcSrc2)) {
				//encode the temp value
				memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
				pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc1);
				strcat(ATresponse, pcSrc1);	//add encoded temp value
				pcTmp = strtok(NULL, ",|&");
			}

			//check if we can start with next temp series
			if ((pcTmp) && (pcTmp < pcData)) {
				strcat(ATresponse, ",");
				iIdx++;	//start with next sensor
				pcSrc2 = strstr(&pcTmp[strlen(pcTmp) + 1], "|"); //adjust the last postion to next sensor end
				if (!pcSrc2) {
					//no more temperature series available
					pcSrc2 = pcData;
				}
			}

		}
	}

	pcSrc1 = pcTmp + strlen(pcTmp) + 1;	//pcTmp is b=yyy
	//check if battery has series values
	if (*pcSrc1 != 'p') {
		pcSrc2 = strstr(pcSrc1, "&");	//end of battery
		pcTmp = strtok(pcSrc1, "&");	//get all series values except the first
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, ",");
			pcSrc1 = strrchr(pcTmp, ','); //postion to the last battery level  (e.g pcTmp 100 or 100,100,..
			if (pcSrc1) {
				strcat(ATresponse, pcSrc1 + 1); //past the last comma
			} else {
				strcat(ATresponse, pcTmp); //no comma
			}
			strcat(ATresponse, ",");

			pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
			if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
				strcat(ATresponse, pcSrc1 + 1);
			} else {
				//power plugged does not have series values
				pcTmp = pcSrc2 + 1;
				strcat(ATresponse, &pcTmp[2]); //pcTmp contains p=yyy
			}

		}

	} else {
		//battery does not have series values
		strcat(ATresponse, ",");
		strcat(ATresponse, &pcTmp[2]); //pcTmp contains b=yyy

		strcat(ATresponse, ",");
		pcSrc2 = pcTmp + strlen(pcTmp);  //end of battery
		pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
		if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
			strcat(ATresponse, pcSrc1 + 1);	//last power plugged values is followed by null
		} else {
			//power plugged does not have series values
			pcTmp = pcSrc2 + 1;
			strcat(ATresponse, &pcTmp[2]); //pcTmp contains p=yyy
		}

	}
	iIdx = strlen(ATresponse);
	ATresponse[iIdx] = 0x1A;

}

void processmsg(char* pSMSmsg) {
	char* pcTmp = NULL;
	int8_t iCnt = 0;

	pcTmp = strtok(pSMSmsg, ",");
	if (pcTmp) {
		//get msg type
		switch (pcTmp[3]) {
			case '1':
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					pstCfgInfoA = ScratchPad;
					memcpy(pstCfgInfoA, INFOA_ADDR, sizeof(CONFIG_INFOA));
					//get & set gateway
					uart_tx("AT+CSCA=\"");
					uart_tx(pcTmp);
					uart_tx("\",145\r\n");
					delay(1000);

					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						//get upload mode
						pstCfgInfoA->cfgUploadMode = strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							//get APN
							memcpy(pstCfgInfoA->cfgAPN, pcTmp, strlen(pcTmp));
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								//get upload interval
								g_iUploadPeriod = strtol(pcTmp, 0, 10);
								pcTmp = strtok(NULL, ",");
								if (pcTmp) {
									g_iSamplePeriod = strtol(pcTmp, 0, 10);
									pcTmp = strtok(NULL, ",");
								}
							}
						}
					}

					if (pcTmp) {
						if (strtol(pcTmp, 0, 10)) {
							iStatus |= RESET_ALERT;
						} else {
							iStatus &= ~RESET_ALERT;
						}
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							if (strtol(pcTmp, 0, 10)) {
								iStatus |= ENABLE_SLOT1;
							} else {
								iStatus &= ~ENABLE_SLOT1;
							}
						}

						FRAMCtl_write8(pstCfgInfoA, INFOA_ADDR,
								sizeof(CONFIG_INFOA));
					}
				}
				break;
			case '2':
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					pstCfgInfoA = ScratchPad;
					memcpy(pstCfgInfoA, INFOA_ADDR, sizeof(CONFIG_INFOA));
				}
				for (iCnt = 0; (iCnt < MAX_NUM_SENSORS) && (pcTmp); iCnt++) {
					pstCfgInfoA->stTempAlertParams[iCnt].mincold = strtod(pcTmp,
							NULL);
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						pstCfgInfoA->stTempAlertParams[iCnt].threshcold =
								strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							pstCfgInfoA->stTempAlertParams[iCnt].minhot =
									strtod(pcTmp, NULL);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								pstCfgInfoA->stTempAlertParams[iCnt].threshhot =
										strtol(pcTmp, 0, 10);
								pcTmp = strtok(NULL, ",");
							}
						}
					}
				}
				if (pcTmp) {
					pstCfgInfoA->stBattPowerAlertParam.minutespower = strtol(
							pcTmp, 0, 10);
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						pstCfgInfoA->stBattPowerAlertParam.enablepoweralert =
								strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							pstCfgInfoA->stBattPowerAlertParam.minutesbathresh =
									strtol(pcTmp, 0, 10);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								pstCfgInfoA->stBattPowerAlertParam.battthreshold =
										strtol(pcTmp, 0, 10);
							}
						}
					}
					FRAMCtl_write8(pstCfgInfoA, INFOA_ADDR,
							sizeof(CONFIG_INFOA));
				}
				break;
			case '3':
				iCnt = 0;
#if 0
				pcTmp = strtok(NULL,",");

				if(pcTmp)
				{
					pstCfgInfoB = ScratchPad;
					memcpy(pstCfgInfoB,INFOB_ADDR,sizeof(CONFIG_INFOB));
					pstCfgInfoB->iDAcount=0;
					for(iCnt; (iCnt < MAX_SMS_NUM) && (pcTmp); iCnt++)
					{
						memcpy(pstCfgInfoB->cfgSMSDA[iCnt],pcTmp,strlen(pcTmp));
						pcTmp = strtok(NULL,",");
					}
					pstCfgInfoB->iDAcount = iCnt;
					FRAMCtl_write8(pstCfgInfoB,INFOB_ADDR,sizeof(CONFIG_INFOB));
				}
#endif
				break;
			case '4':
				break;

		}
	}
}
#ifdef SPI_NOR_TEST
void spi_init()
{
	SPI_TX_6(SPI_READID_CMD, 0, 0, 0, 0, 0);
	spi_transfer();

}

void spi_read(unsigned int addr, unsigned char* pData, unsigned int len)
{
	int iIdx = 0;

	//input check

	while (iIdx < len)
	{
		SPI_TX_5(SPI_READDATA_CMD, ((addr >> 16) & 0xFF), ((addr >> 8) & 0xFF), (addr & 0xFF), 0);
		spi_transfer();

		pData[iIdx] = RX[RXTailIdx -1];	//read the data
		RXHeadIdx = RXTailIdx;//update the head
		iIdx++;
		addr++;
	}

}

void spi_write(unsigned int addr, unsigned char* pData, unsigned int len)
{
	int iIdx = 0;
	int bytestowrite = 0;

	//input check
	do
	{
		if(!(addr % 4096 ))
		{
			//write enable
			SPI_TX_1(SPI_WREN_CMD);
			delay(10);
			spi_transfer();
			//erase the 4K sector
			SPI_TX_4(SPI_ERASE4K_CMD, ((addr >> 16) & 0xFF), ((addr >> 8) & 0xFF), (addr & 0xFF));
			spi_transfer();
			delay(300);
		}

		if(len > PAGE_SIZE)
		{
			bytestowrite = PAGE_SIZE;
			len -= PAGE_SIZE;
		}
		else
		{
			bytestowrite = len;
			len -= bytestowrite;
		}

		//write enable
		SPI_TX_1(SPI_WREN_CMD);
		delay(10);
		spi_transfer();
		SPI_TX_PROGRAM(addr, pData, bytestowrite);
		spi_transfer();
		delay(100);
		addr += bytestowrite;

	}while (len);

}

void spi_transfer()
{
	TXIdx = 1;
	iTXInProgress = 1;				//next message is ready

	P2OUT &= ~BIT3;//CS is held low
	UCA1TXBUF = TX[0];//Transmit the first byte to trigger the SPI clk generator
	while(iTXInProgress != 0);//Wait till the whole data is transferred
	P2OUT |= BIT3;//CS is held high

}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(UCA1IV, USCI_SPI_UCTXIFG))
	{
		case USCI_NONE: break;
		case USCI_SPI_UCRXIFG:
		if(UCA1STATW & UCOE)
		{
			iError++;
		}
		RX[RXTailIdx] = UCA1RXBUF;
		RXTailIdx = (RXTailIdx + 1);
		if(RXTailIdx >= RX_LEN)
		{
			RXTailIdx = 0;
		}
		//__bic_SR_register_on_exit(LPM0_bits); // Wake up to setup next TX
		break;
		case USCI_SPI_UCTXIFG:
		if((TXIdx < iTxLen) && (iTXInProgress))
		{
			UCA1TXBUF = TX[TXIdx];
			TXIdx = (TXIdx + 1);
		}
		else
		{
			//UCA1IE &= ~UCTXIE;
			iTXInProgress = 0;
		}
		break;
		default: break;
	}
}
#endif
#if 0
char* itoa(int num, int digits)
{
	uint8_t digit;
	uint8_t iIdx = digits-1;

	while (num != 0)
	{
		digit = (num % 10) + 0x30;
		str[iIdx] = digit;
		iIdx--;
		num = num/10;

	}

	return str;
}
#endif
void sendhb() {
	char* pcTmp = NULL;

	//send heart beat
	memset(SampleData, 0, sizeof(SampleData));
	strcat(SampleData, SMS_HB_MSG_TYPE);
	strcat(SampleData, "358072043120127");
	strcat(SampleData, ",");
	if (iStatus & ENABLE_SLOT1) {
		strcat(SampleData, "1,");
	} else {
		strcat(SampleData, "0,");
	}

	strcat(SampleData, DEF_GW);
	strcat(SampleData, ",");

	strcat(SampleData, "1,1,1,1,");	//ZZZZ to be changed based on jack detection

	pcTmp = itoa(batt_getlevel());	//opt by directly using tmpstr
	strcat(SampleData, pcTmp);
	if (P4IN & BIT4) {
		strcat(SampleData, ",0");
	} else {
		strcat(SampleData, ",1");
	}

	SampleData[strlen(SampleData)] = 0x1A;
	sendmsg(SampleData);
}

void writetoI2C(uint8_t addressI2C, uint8_t dataI2C) {
#ifndef I2C_DISABLED
	delay(100);
	i2c_write(SLAVE_ADDR_DISPLAY, addressI2C, 1, &dataI2C);
#endif

}

// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) Port_2 (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(P2IV, P2IV_P2IFG7))
	{
		case P2IV_NONE: break;
		case P2IV_P2IFG0: break;
		case P2IV_P2IFG1:
		break;
		case P2IV_P2IFG2:
		iSlot = !iSlot;
		break;

		case P2IV_P2IFG3: break;
		case P2IV_P2IFG4: break;
		case P2IV_P2IFG5: break;
		case P2IV_P2IFG6: break;
		case P2IV_P2IFG7: break;
		default: break;
	}
}
