/*
 * calibration.c
 *
 *  Created on: May 20, 2015
 *      Author: sergioam
 */

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
#include "stringutils.h"
#include "common.h"
#include "driverlib.h"
#include "globals.h"
#include "lcd.h"
#include "string.h"
#include "stdio.h"
#include "modem.h"
#include "sms.h"

#define  CALIBRATE 4	//0 - default, 1 = device A, 2 = device B...
//#define SMS_TEST
#define LCD_TEST
#define ENABLE_SIM_SLOT		//needed to set on new board, comment it for old board

// Num of loops to do while SMS testing
#define NUM_SMS_TESTS 10

extern volatile uint32_t iMinuteTick;

//len constants

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

//iStatus contants
#define ENABLE_SLOT1	0x0010
#define RESET_ALERT  	0x0020

#define RX_EXTENDED_LEN 300		//to retreive a complete http get cfg message

#define MODEM_POWERED_ON 	0x0001
#define MODEM_CHECK_RETRY 	3

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
volatile char RXBuffer[RX_LEN];
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
int32_t timedate;

extern CONFIG_INFOA* g_pInfoA;
extern CONFIG_INFOB* g_pInfoB;

extern char __STACK_END; /* the type does not matter! */
extern char __STACK_SIZE; /* the type does not matter! */
volatile int8_t iSlot = 1;

int main_calibration(void) {

	int t = 0;
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
	UCA1IE |= UCRXIE | UCTXIE;// Enable USCI_A0 RX, TX interrupt
#endif

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

#ifdef LCD_TEST
	//test the LCD
	memset(TMP, 0, sizeof(TMP));
	lcd_reset();
	lcd_blenable();
	g_iLCDVerbose = VERBOSE_BOOTING;
	lcd_init();

	__bis_SR_register(GIE);

	delay(1000);
	lcd_clear();
	lcd_print_lne("CALIBRATION MODE", LINE1);
	delay(1000);
	lcd_print_lne("V(" __DATE__ ")",LINE2);
	delay(2000);

	i2c_init(380000);

#endif

#ifdef SMS_TEST
	int8_t g_iStatus = 0;
	iIdx = 0;

	//check Modem is powered on
	for (iIdx = 0; iIdx < MODEM_CHECK_RETRY; iIdx++) {
		if ((P4IN & BIT0) == 0) {
			g_iStatus |= MODEM_POWERED_ON;
			break;
		} else {
			g_iStatus &= ~MODEM_POWERED_ON;
			delay(100);
		}
	}

	if (g_iStatus & MODEM_POWERED_ON)
	for (t = 0; t < NUM_SMS_TESTS; t++) {
		modem_init();
		modem_getSimCardInfo();
#if 1
		sprintf(ATresponse, "IMEI %s Calibration test %d for %s ",
				g_pInfoA->cfgIMEI, t, g_pInfoA->cfgSMSCenter[iSlot]);
		sendmsg(ATresponse);
		delay(1000);
#endif
		uart_tx("AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n");
		delay(MODEM_TX_DELAY2);

		uart_tx("AT#SGACT=1,1\r\n");
		delay(MODEM_TX_DELAY2);

		uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
		//uart_tx("AT#HTTPCFG=1,\"54.175.222.246\",80\r\n");

		delay(MODEM_TX_DELAY2);

		//reset the RX counters to reuse memory from POST DATA
		RXTailIdx = RXHeadIdx = 0;
		iRxLen = RX_EXTENDED_LEN;
		RXBuffer[RX_EXTENDED_LEN + 1] = 0;	//null termination

		memset(ATresponse, 0, sizeof(ATresponse));
		strcpy(ATresponse,
				"AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/358072043119046/1/\"\r\n");
		//strcpy(ScratchPad,"AT#HTTPQRY=1,0,\"/ip\"\r\n");

		uart_tx(ATresponse);
		delay(10000);
		uart_tx("AT#HTTPRCV=1\r\n");
		delay(10000);

		uart_rx_cleanBuf(ATCMD_HTTPQRY, ATresponse, sizeof(ATresponse));
		delay(5000);

		//uart_tx("AT+CPMS=\"SM\"\r\n");
		//delay(5000);
		// uart_tx("AT+CNMI?\r\n");
		//delay(5000);
		// uart_tx("AT+CPMS?\r\n");
		// delay(5000);
		g_pInfoA->cfgSIMSlot = !g_pInfoA->cfgSIMSlot;
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

/*
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
			processmsg_local(ATresponse);
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
			"IMEI=358072043119046&ph=00447751035864&v=1.20140817.1&sid=0|1|2|3&sdt=2015/03/03:08:22:08&i=1&t=25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7|25.1,26.1,25.9,24.3,23.7,25.1,26.1,25.9,24.3,23.7&b=100,100,100,100,100,100,100,100,100,100&p=1,1,1,1,1,1,1,1,1,1");
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
		strncat(ATresponse, &pcSrc1[2], 4);	//get first sensor temp value
		strcat(ATresponse, ",1,");
		strncat(ATresponse, &pcSrc1[7], 4);	//get second sensor temp value
		strcat(ATresponse, ",2,");
		strncat(ATresponse, &pcSrc1[12], 4);	//get second sensor temp value
		strcat(ATresponse, ",3,");
		strncat(ATresponse, &pcSrc1[17], 4);	//get second sensor temp value
		strcat(ATresponse, ",");
		pcTmp = strtok(pcSrc1, "&");	//past the temperature fields
		pcTmp = strtok(NULL, ",&");	//past the first battery value
	} else {
		iIdx = 0;
		pcSrc2 = strstr(pcSrc1, "|");	//end of first sensor
		if (!pcSrc2)
			pcSrc2 = pcData;
		pcTmp = strtok(&pcSrc1[2], ",|&");	//get first temp value

		while ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(ATresponse, itoa_nopadding(iIdx));	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			pcSrc1 = &ATresponse[SMS_ENCODED_LEN];	//reuse
			encode(strtod(pcTmp, NULL), pcSrc1);
			strcat(ATresponse, pcSrc1);	//add encoded temp value

			pcTmp = strtok(NULL, ",|&");
			while ((pcTmp) && (pcTmp < pcSrc2)) {
				//encode the temp value
				memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
				pcSrc1 = &ATresponse[SMS_ENCODED_LEN];	//reuse
				encode(strtod(pcTmp, NULL), pcSrc1);
				strcat(ATresponse, pcSrc1);	//add encoded temp value
				pcTmp = strtok(NULL, ",|&");
			}

			//check if we can start with next temp series
			if ((pcTmp) && (pcTmp < pcData)) {
				strcat(ATresponse, ",");
				iIdx++;	//start with next sensor
				pcSrc2 = strstr(&pcTmp[strlen(pcTmp) + 1], "|");//adjust the last postion to next sensor end
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
		pcSrc2 = pcTmp + strlen(pcTmp); //end of battery
		pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
		if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
			strcat(ATresponse, pcSrc1 + 1); //last power plugged values is followed by null
		} else {
			//power plugged does not have series values
			pcTmp = pcSrc2 + 1;
			strcat(ATresponse, &pcTmp[2]); //pcTmp contains p=yyy
		}

	}
	iIdx = strlen(ATresponse);
	ATresponse[iIdx] = 0x1A;
*/

	return 1;
}

void processmsg_local(char* pSMSmsg) {
	char* pcTmp = NULL;
	int8_t iCnt = 0;

	pcTmp = strtok(pSMSmsg, ",");
	if (pcTmp) {
		//get msg type
		switch (pcTmp[3]) {
		case '1':
			pcTmp = strtok(NULL, ",");
			if (pcTmp) {
				//get & set gateway
				uart_tx("AT+CSCA=\"");
				uart_tx(pcTmp);
				uart_tx("\",145\r\n");
				delay(1000);

				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					//get upload mode
					g_pInfoA->cfgUploadMode = strtol(pcTmp, 0, 10);
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						//get APN
						memcpy(g_pInfoA->cfgAPN, pcTmp, strlen(pcTmp));
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
						g_iStatus |= RESET_ALERT;
					} else {
						g_iStatus &= ~RESET_ALERT;
					}
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						if (strtol(pcTmp, 0, 10)) {
							g_iStatus |= ENABLE_SLOT1;
						} else {
							g_iStatus &= ~ENABLE_SLOT1;
						}
					}
				}
			}
			break;
		case '2':
			pcTmp = strtok(NULL, ",");
			for (iCnt = 0; (iCnt < MAX_NUM_SENSORS) && (pcTmp); iCnt++) {
				g_pInfoA->stTempAlertParams[iCnt].mincold = strtod(pcTmp,
				NULL);
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					g_pInfoA->stTempAlertParams[iCnt].threshcold = strtol(
							pcTmp, 0, 10);
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						g_pInfoA->stTempAlertParams[iCnt].minhot = strtod(
								pcTmp,
								NULL);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							g_pInfoA->stTempAlertParams[iCnt].threshhot =
									strtol(pcTmp, 0, 10);
							pcTmp = strtok(NULL, ",");
						}
					}
				}
			}
			if (pcTmp) {
				g_pInfoA->stBattPowerAlertParam.minutespower = strtol(pcTmp,
						0, 10);
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					g_pInfoA->stBattPowerAlertParam.enablepoweralert =
							strtol(pcTmp, 0, 10);
					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						g_pInfoA->stBattPowerAlertParam.minutesbathresh =
								strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							g_pInfoA->stBattPowerAlertParam.battthreshold =
									strtol(pcTmp, 0, 10);
						}
					}
				}
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

void writetoI2C(uint8_t addressI2C, uint8_t dataI2C) {
#ifndef I2C_DISABLED
	delay(100);
	i2c_write(SLAVE_ADDR_DISPLAY, addressI2C, 1, &dataI2C);
#endif

}

/*
 // Port 2 interrupt service routine
 #if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
 #pragma vector=PORT2_VECTOR
 __interrupt void Port_2_calibration(void)
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
 */

