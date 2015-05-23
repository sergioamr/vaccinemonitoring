/* Copyright (c) 2015, Intel Corporation. All rights reserved.
 *
 * INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL� PRODUCTS. NO LICENSE, EXPRESS OR IMPLIED,
 * BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS IS GRANTED BY THIS DOCUMENT. EXCEPT AS PROVIDED
 * IN INTEL'S TERMS AND CONDITIONS OF SALE FOR SUCH PRODUCTS, INTEL ASSUMES NO LIABILITY WHATSOEVER, AND INTEL
 * DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR USE OF INTEL PRODUCTS INCLUDING LIABILITY
 * OR WARRANTIES RELATING TO FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT OF ANY PATENT,
 * COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.
 * UNLESS OTHERWISE AGREED IN WRITING BY INTEL, THE INTEL PRODUCTS ARE NOT DESIGNED NOR INTENDED FOR ANY APPLICATION
 * IN WHICH THE FAILURE OF THE INTEL PRODUCT COULD CREATE A SITUATION WHERE PERSONAL INJURY OR DEATH MAY OCCUR.
 *
 * Intel may make changes to specifications and product descriptions at any time, without notice.
 * Designers must not rely on the absence or characteristics of any features or instructions marked
 * "reserved" or "undefined." Intel reserves these for future definition and shall have no responsibility
 * whatsoever for conflicts or incompatibilities arising from future changes to them. The information here
 * is subject to change without notice. Do not finalize a design with this information.
 * The products described in this document may contain design defects or errors known as errata which may
 * cause the product to deviate from published specifications. Current characterized errata are available on request.
 *
 * This document contains information on products in the design phase of development.
 * All Thermal Canyons featured are used internally within Intel to identify products
 * that are in development and not yet publicly announced for release.  Customers, licensees
 * and other third parties are not authorized by Intel to use Thermal Canyons in advertising,
 * promotion or marketing of any product or services and any such use of Intel's internal
 * Thermal Canyons is at the sole risk of the user.
 *
 */

#define HTTP_RESPONSE_RETRY	10

#define MAX_NUM_CONTINOUS_SAMPLES 10

#define TS_SIZE				21
#define TS_FIELD_OFFSET		1	//1 - $, 3 - $TS

#define NOTFROMFILE

#include "thermalcanyon.h"

volatile char isConversionDone = 0;
volatile uint8_t iDisplayId = 0;
volatile int iSamplesRead = 0;

_Sigfun * signal(int i, _Sigfun *proc) {
	__no_operation();
	return NULL;
}

static void setupIO() {

	// Configure GPIO
	P2DIR |= BIT3;							// SPI CS
	P2OUT |= BIT3;					// drive SPI CS high to deactive the chip
	P2SEL1 |= BIT4 | BIT5 | BIT6;             // enable SPI CLK, SIMO, SOMI
	PJSEL0 |= BIT4 | BIT5;                    // For XT1
	P1SELC |= BIT0 | BIT1;                    // Enable VEREF-,+
	P1SELC |= BIT2;                           // Enable A/D channel A2
#ifdef SEQUENCE
	P1SELC |= BIT3 | BIT4 | BIT5;          // Enable A/D channel A3-A5
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
	P4SELC |= BIT2;          				// Enable A/D channel A10
#endif
#endif
	P1SEL1 |= BIT6 | BIT7;					// Enable I2C SDA and CLK

	P2SEL1 |= BIT0 | BIT1;                    // USCI_A0 UART operation
	P2SEL0 &= ~(BIT0 | BIT1);
	P4DIR |= BIT0 | BIT5 | BIT7; // Set P4.0 (Modem reset), LEDs to output direction
	P4OUT &= ~BIT0;                           // Reset high

	P3DIR |= BIT4;      						// Set P3.4 buzzer output
	P4IE |= BIT1;							// enable interrupt for button input
	P2IE |= BIT2;							// enable interrupt for buzzer off

	lcd_setupIO();

#ifdef POWER_SAVING_ENABLED
	P3DIR |= BIT3;							// Modem DTR
	P3OUT &= ~BIT3;// DTR ON (low)
#endif
	//P1SEL0 |= BIT2;

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

	// Configure ADC12
	ADC12CTL0 = ADC12ON | ADC12SHT0_2;       // Turn on ADC12, set sampling time
	ADC12CTL1 = ADC12SHP;                     // Use pulse mode
	ADC12CTL2 = ADC12RES_2;                   // 12bit resolution
	ADC12CTL3 |= ADC12CSTARTADD_2;			// A2 start channel
	ADC12MCTL2 = ADC12VRSEL_4 | ADC12INCH_2; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 2
#ifdef SEQUENCE
	ADC12MCTL3 = ADC12VRSEL_4 | ADC12INCH_3; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 3
	ADC12MCTL4 = ADC12VRSEL_4 | ADC12INCH_4; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 4
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
	ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
	ADC12MCTL6 = ADC12VRSEL_4 | ADC12INCH_10 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#else
			ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#endif
	ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;                   // sample a sequence
	ADC12CTL0 |= ADC12MSC; //first sample by trigger and rest automatic trigger by prior conversion
#endif
	//ADC interrupt logic
	//TODO comment ADC for debugging other interfaces
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
	ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5 | ADC12IE6; // Enable ADC conv complete interrupt
#else
			ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5; // Enable ADC conv complete interrupt
#endif

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

#ifndef I2C_DISABLED
	//i2c_init(EUSCI_B_I2C_SET_DATA_RATE_400KBPS);
	i2c_init(380000);

#endif

	__bis_SR_register(GIE);		//enable interrupt globally
}

void pullTime() {
	uart_resetbuffer();
	uart_tx("AT+CCLK?\r\n");

	memset(ATresponse, 0, sizeof(ATresponse));
	uart_rx(ATCMD_CCLK, ATresponse);
	parsetime(ATresponse, &currTime);
	rtc_init(&currTime);
	if(g_iCurrYearDay != currTime.tm_yday) {
		// Day has changed so save the new date TODO keep trying until date is set. Call function ONCE PER DAY
		g_iCurrYearDay = currTime.tm_yday;
	}
}

int main(void) {

	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	uint32_t iIdx = 0;
	int iPOSTstatus = 0;
	int32_t dwLastseek = 0;
	int32_t dwFinalSeek = 0;
	int32_t iSize = 0;
	int16_t iOffset = 0;

#ifdef _DEBUG
	int8_t dummy = 0;  // Used to calculate the size of the different elements consuming memory
#endif

	int8_t iSampleCnt = 0;
	char gprs_network_indication = 0;
	int32_t dw_file_pointer_back_log = 0; // for error condition /// need to be tested.
	char file_pointer_enabled_sms_status = 0; // for sms condtition enabling.../// need to be tested

	config_Init();	// Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.

	WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT

#ifdef _DEBUG
	//TODO for stack usage calculation. Should be removed during the release
	//*(int32_t*)(&__STACK_END - &__STACK_SIZE) = 0xdeadbeef;
	iIdx = (unsigned long) &__STACK_SIZE;
	memset((void*) (&__STACK_END - &__STACK_SIZE), 0xa5, iIdx / 4); //paint 1/4th stack with know values

	dummy = (int8_t) sizeof(CONFIG_INFOA);
	dummy = (int8_t) sizeof(CONFIG_INFOB);
	dummy = (int8_t) sizeof(Temperature);
#endif

	setupIO();
	delay(100);

	lcd_reset();
	lcd_blenable();

#ifndef BATTERY_DISABLED
	batt_init();
#endif
	delay(100);

	lcd_init();
	delay(100);

	g_iDebug_state = 0;         // Setting up debug states
	lcd_print("Booting...");
	delay(100);
#ifndef _DEBUG
	lcd_print_line("(v)" __DATE__, LINE2);
#else
	lcd_print_line("(db)" __TIME__, LINE2);
#endif

	//Configure 7 segement LEDs
	writetoI2C(0x01, 0x0f);						// Decode Mode register
	writetoI2C(0x02, 0x0B);						//Intensity register
	writetoI2C(0x04, 0x01);						//Configuration Register
	writetoI2C(0x20, 0x0A);						//Display channel

#ifdef NO_CODE_SIZE_LIMIT
	writetoI2C(0x21, 4);
	writetoI2C(0x22, 2);
	writetoI2C(0x23, 0);
	writetoI2C(0x24, 0x04);
#endif

	sampletemp();
#ifndef _DEBUG
	delay(2000);  // to allow conversion to get over and prevent any side-effects to other interface like modem
	 	 	 	  // TODO is this delay to help on the following bug from texas instruments ? (http://www.ti.com/lit/er/slaz627b/slaz627b.pdf)
#endif

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

	if (iStatus & MODEM_POWERED_ON) {
		modem_init(g_pInfoA->cfgSIMSlot);

#ifdef POWER_SAVING_ENABLED
		uart_tx("AT+CFUN=5\r\n");//full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);

		uart_tx("AT+CFUN?\r\n");//full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);
#endif

		pullTime();

		modem_getsimcardsinfo();
		modem_checkSignal();

		lcd_print("Checking GPRS");
		/// added for gprs connection..//
		signal_gprs = dopost_gprs_connection_status(GPRS);

		lcd_print("IMEI");

		// added for IMEI number//
		if ((uint8_t) g_pInfoA->cfgIMEI[0] == 0xFF || (uint8_t) g_pInfoA->cfgIMEI[0] == '+') {
			uart_resetbuffer();
			uart_tx("AT+CGSN\r\n");
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CGSN, ATresponse);
			if (memcmp(ATresponse, "INVALID", strlen("INVALID"))) {
				memcpy(g_pInfoA->cfgIMEI, ATresponse, strlen(ATresponse) + 1);
			}
		}

		// Reading the Service Center Address to use as message gateway
		// http://www.developershome.com/sms/cscaCommand.asp
		// Get service center address; format "+CSCA: address,address_type"

		//uart_resetbuffer();
		// Disable echo from modem
		uart_tx("ATE0\r\n");
		//heartbeat
		sendhb();
	}

	lcd_print("Battery check");

#ifndef BATTERY_DISABLED
	iBatteryLevel = batt_getlevel();
#else
	iBatteryLevel = 0;
#endif

	if (iBatteryLevel == 0)
		lcd_print("Battery FAIL");
	else if (iBatteryLevel > 100)
		lcd_print("Battery UNKNOWN");
	else if (iBatteryLevel > 99)
		lcd_print("Battery FULL");
	else if (iBatteryLevel > 15)
		lcd_print("Battery OK");
	else if (iBatteryLevel)
		lcd_print("Battery LOW");

	delay(1000);
	/* Register work area to the default drive */
	f_mount(&FatFs, "", 0);

	//get the last read offset from FRAM
	if (g_pInfoB->dwLastSeek > 0) {
		dwLastseek = g_pInfoB->dwLastSeek;
	}

	iUploadTimeElapsed = iMinuteTick;		//initialize POST minute counter
	iSampleTimeElapsed = iMinuteTick;
	iSMSRxPollElapsed = iMinuteTick;
	iLCDShowElapsed = iMinuteTick;
	iMsgRxPollElapsed = iMinuteTick;
	iDisplayId = 0;

	// Init finished, we disabled the debugging display
	g_iDebug_state = -1;

	while (1) {
		//check if conversion is complete
		if ((isConversionDone) || (iStatus & TEST_FLAG)) {
			//convert the current sensor ADC value to temperature
			for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
				memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
				ConvertADCToTemperature(ADCvar[iIdx], &Temperature[iIdx][0], iIdx);
			}

			//Write temperature data to 7 segment display
			writetoI2C(0x20, SensorDisplayName[iDisplayId]);//Display channel
			writetoI2C(0x21, Temperature[iDisplayId][0]);//(A0tempdegCint >> 8));
			writetoI2C(0x22, Temperature[iDisplayId][1]);//(A0tempdegCint >> 4));
			writetoI2C(0x23, Temperature[iDisplayId][3]);	//(A0tempdegCint));
			writetoI2C(0x24, 0x04);

#ifndef  NOTFROMFILE
			//log to file
			fr = logsampletofile(&filw, (int *) &iBytesLogged);
			if (fr == FR_OK) {
				iSampleCnt++;
				if (iSampleCnt >= MAX_NUM_CONTINOUS_SAMPLES) {
					iStatus |= LOG_TIME_STAMP;
					iSampleCnt = 0;
				}
			}
#endif
			//monitor for temperature alarms
			monitoralarm();

#ifdef SMS
			memset(MSGData,0,sizeof(MSGData));
			strcat(MSGData,SensorName[iDisplayId]);
			strcat(MSGData," ");
			strcat(MSGData,Temperature);
			iIdx = strlen(MSGData);
			MSGData[iIdx] = 0x1A;
#endif
			//sendmsg(MSGData);
			isConversionDone = 0;

			//if(iIdx >= SAMPLE_PERIOD)
			//if(((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) || (iStatus & TEST_FLAG) || (iStatus & ALERT_UPLOAD_ON))
			if ((((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod)
					|| (iStatus & TEST_FLAG) || (iStatus & BACKLOG_UPLOAD_ON)
					|| (iStatus & ALERT_UPLOAD_ON))
					&& !(iStatus & NETWORK_DOWN)) {
				lcd_print_line("Transmitting....", LINE2);
				//iStatus &= ~TEST_FLAG;
#ifdef SMS_ALERT
				iStatus &= ~SMSED_HIGH_TEMP;
				iStatus &= ~SMSED_LOW_TEMP;
#endif
				if ((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) {
					//if(((g_iUploadPeriod/g_iSamplePeriod) < MAX_NUM_CONTINOUS_SAMPLES) ||
					//    (iStatus & ALERT_UPLOAD_ON))
					if ((g_iUploadPeriod / g_iSamplePeriod)
							< MAX_NUM_CONTINOUS_SAMPLES) {
						//trigger a new timestamp
						iStatus |= LOG_TIME_STAMP;
						iSampleCnt = 0;
						//iStatus &= ~ALERT_UPLOAD_ON;	//reset alert upload indication
					}

					iUploadTimeElapsed = iMinuteTick;
				} else if ((iStatus & ALERT_UPLOAD_ON)
						&& (iSampleCnt < MAX_NUM_CONTINOUS_SAMPLES)) {
					//trigger a new timestamp
					iStatus |= LOG_TIME_STAMP;
					iSampleCnt = 0;
				}

				//reset the alert uplaod indication
				if (iStatus & ALERT_UPLOAD_ON) {
					iStatus &= ~ALERT_UPLOAD_ON;
				}

#ifdef POWER_SAVING_ENABLED
				modem_exit_powersave_mode();
#endif

				if (!(iStatus & TEST_FLAG)) {

					uart_tx("AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n"); //APN
					//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#SGACT=1,1\r\n");
					delay(MODEM_TX_DELAY2);

					uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
					delay(MODEM_TX_DELAY2);
				}

#ifdef NOTFROMFILE
				iPOSTstatus = 0;	//set to 1 if post and sms should happen
				memset(SampleData,0,sizeof(SampleData));
				strcat(SampleData,"IMEI=358072043113601&phone=8455523642&uploadversion=1.20140817.1&sensorid=0|1|2|3&");//SERIAL
				rtc_get(&currTime);
				strcat(SampleData,"sampledatetime=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,itoa(currTime.tm_year)); strcat(SampleData,"/");
					strcat(SampleData,itoa(currTime.tm_mon)); strcat(SampleData,"/");
					strcat(SampleData,itoa(currTime.tm_mday)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_hour)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_min)); strcat(SampleData,":");
					strcat(SampleData,itoa(currTime.tm_sec));
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				strcat(SampleData,"interval=10|10|10|10&");

				strcat(SampleData,"temp=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,&Temperature[iIdx][0]);
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				pcData = itoa(batt_getlevel());;
				strcat(SampleData,"batterylevel=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,pcData);
					if(iIdx != (MAX_NUM_SENSORS - 1))
					{
						strcat(SampleData, "|");
					}
				}
				strcat(SampleData,"&");

				if(P4IN & BIT4)
				{
					strcat(SampleData,"powerplugged=0|0|0|0");
				}
				else
				{
					strcat(SampleData,"powerplugged=1|1|1|1");
				}
#else
				//disable UART RX as RX will be used for HTTP POST formating
				pcTmp = pcData = pcSrc1 = pcSrc2 = NULL;
				iIdx = 0;
				iPOSTstatus = 0;
				fr = FR_DENIED;
				iOffset = 0;
				char* fn = getCurrentFileName(&currTime);

				//fr = f_read(&filr, acLogData, 1, &iIdx);  /* Read a chunk of source file */
				memset(ATresponse, 0, AGGREGATE_SIZE + 1); //ensure the buffer in aggregate_var section is more than AGGREGATE_SIZE
				fr = f_open(&filr, fn, FA_READ | FA_OPEN_ALWAYS);
				if (fr == FR_OK) {
					dw_file_pointer_back_log = dwLastseek; // added for dummy storing///
					//seek if offset is valid and not greater than existing size else read from the beginning
					if ((dwLastseek != 0) && (filr.fsize >= dwLastseek)) {
						f_lseek(&filr, dwLastseek);
					} else {
						dwLastseek = 0;
					}

					iOffset = dwLastseek % SECTOR_SIZE;
					//check the position is in first half of sector
					if ((SECTOR_SIZE - iOffset) > AGGREGATE_SIZE) {
						fr = f_read(&filr, ATresponse, AGGREGATE_SIZE, &iIdx); /* Read first chunk of sector*/
						if ((fr == FR_OK) && (iIdx > 0)) {
							iStatus &= ~SPLIT_TIME_STAMP;//clear the last status of splitted data
							pcData = FatFs.win;	//reuse the buffer maintained by the file system
							//check for first time stamp
							//pcTmp = strstr(&pcData[iOffset],"$TS");
							pcTmp = strstr(&pcData[iOffset], "$");//to prevent $TS rollover case
							if ((pcTmp) && (pcTmp < &pcData[SECTOR_SIZE])) {
								iIdx = pcTmp; //start position
								//check for second time stamp
								//pcTmp = strstr(&pcTmp[TS_FIELD_OFFSET],"$TS");
								pcTmp = strstr(&pcTmp[TS_FIELD_OFFSET], "$");
								if ((pcTmp) && (pcTmp < &pcData[SECTOR_SIZE])) {
									iPOSTstatus = pcTmp; //first src end postion
									iStatus &= ~SPLIT_TIME_STAMP; //all data in FATFS buffer
								} else {
									iPOSTstatus = &pcData[SECTOR_SIZE];	//mark first source end position as end of sector boundary
									iStatus |= SPLIT_TIME_STAMP;
								}
								pcTmp = iIdx;//re-initialize to first time stamp

								//is all data within FATFS
								if (!(iStatus & SPLIT_TIME_STAMP)) {
									//first src is in FATFS buffer, second src is NULL
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = dwLastseek
											+ (iPOSTstatus - (int) pcSrc1);	//seek to the next time stamp
								}
								//check to read some more data
								else if ((filr.fsize - dwLastseek)
										> (SECTOR_SIZE - iOffset)) {
									//update the seek to next sector
									dwLastseek = dwLastseek + SECTOR_SIZE
											- iOffset;
									//seek
									//f_lseek(&filr, dwLastseek);
									//fr = f_read(&filr, ATresponse, AGGREGATE_SIZE, &iIdx);  /* Read next data of AGGREGATE_SIZE */
									//if((fr == FR_OK) && (iIdx > 0))
									//if(disk_read_ex(0,ATresponse,filr.dsect+1,AGGREGATE_SIZE) == RES_OK)
									if (disk_read_ex(0, ATresponse,
											filr.dsect + 1, 512) == RES_OK) {
										//calculate bytes read
										iSize = filr.fsize - dwLastseek;
										if (iSize >= AGGREGATE_SIZE) {
											iIdx = AGGREGATE_SIZE;
										} else {
											iIdx = iSize;
										}
										//update final lseek for next sample
										//pcSrc1 = strstr(ATresponse,"$TS");
										pcSrc1 = strstr(ATresponse, "$");
										if (pcSrc1) {
											dwFinalSeek = dwLastseek
													+ (pcSrc1 - ATresponse);//seek to the next TS
											iIdx = pcSrc1;//second src end position
										}
										//no next time stamp found
										else {
											dwFinalSeek = dwLastseek + iIdx;//update with bytes read
											dwLastseek = &ATresponse[iIdx]; //get postion of last byte
											iIdx = dwLastseek; //end position for second src
										}
										//first src is in FATFS buffer, second src is ATresponse
										pcSrc1 = pcTmp;
										pcSrc2 = ATresponse;
									}
								} else {
									//first src is in FATFS buffer, second src is NULL
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = filr.fsize; //EOF - update with file size

								}
							} else {
								//control should not come here ideally
								//update the seek to next sector to skip the bad logged data
								dwLastseek = dwLastseek + SECTOR_SIZE - iOffset;

							}
						} else {
							//file system issue TODO
						}
					} else {
						//the position is second half of sector
						fr = f_read(&filr, ATresponse, SECTOR_SIZE - iOffset,
								&iIdx); /* Read data till the end of sector */
						if ((fr == FR_OK) && (iIdx > 0)) {
							iStatus &= ~SPLIT_TIME_STAMP;//clear the last status of splitted data
							//get position of first time stamp
							//pcTmp = strstr(ATresponse,"$TS");
							pcTmp = strstr(ATresponse, "$");

							if ((pcTmp) && (pcTmp < &ATresponse[iIdx])) {
								//check there are enough bytes to check for second time stamp postion
								if (iIdx > TS_FIELD_OFFSET) {
									//check if all data is in ATresponse
									//pcSrc1 = strstr(&ATresponse[TS_FIELD_OFFSET],"$TS");
									pcSrc1 = strstr(&pcTmp[TS_FIELD_OFFSET],
											"$");
									if ((pcSrc1)
											&& (pcSrc1 < &ATresponse[iIdx])) {
										iPOSTstatus = pcSrc1;//first src end position;
										iStatus &= ~SPLIT_TIME_STAMP;//all data in FATFS buffer
									} else {
										iPOSTstatus = &ATresponse[iIdx]; //first src end position;
										iStatus |= SPLIT_TIME_STAMP;
									}
								} else {
									iPOSTstatus = &ATresponse[iIdx]; //first src end position;
									iStatus |= SPLIT_TIME_STAMP;
								}

								//check if data is splitted across
								if (iStatus & SPLIT_TIME_STAMP) {
									//check to read some more data
									if ((filr.fsize - dwLastseek)
											> (SECTOR_SIZE - iOffset)) {
										//update the seek to next sector
										dwLastseek = dwLastseek + SECTOR_SIZE
												- iOffset;
										//seek
										f_lseek(&filr, dwLastseek);
										fr = f_read(&filr, &dummy, 1, &iIdx); /* dummy read to load the next sector */
										if ((fr == FR_OK) && (iIdx > 0)) {
											pcData = FatFs.win;	//resuse the buffer maintained by the file system
											//update final lseek for next sample
											//pcSrc1 = strstr(pcData,"$TS");
											pcSrc1 = strstr(pcData, "$");
											if ((pcSrc1)
													&& (pcSrc1
															< &pcData[SECTOR_SIZE])) {
												dwFinalSeek = dwLastseek
														+ (pcSrc1 - pcData);//seek to the next TS
												iIdx = pcSrc1;//end position for second src
											} else {
												dwFinalSeek = filr.fsize;//EOF - update with file size
												iIdx = &pcData[dwFinalSeek
														% SECTOR_SIZE]; //end position for second src
											}
											//first src is in ATresponse buffer, second src is FATFS
											pcSrc1 = pcTmp;
											pcSrc2 = pcData;
										}
									} else {
										//first src is in ATresponse buffer, second src is NULL
										pcSrc1 = pcTmp;
										pcSrc2 = NULL;
										//update lseek
										dwFinalSeek = filr.fsize; //EOF - update with file size
									}
								} else {
									//all data in ATresponse
									pcSrc1 = pcTmp;
									pcSrc2 = NULL;
									//update lseek
									dwFinalSeek = dwLastseek
											+ (iPOSTstatus - (int) pcSrc1);	//seek to the next time stamp
								}
							} else {
								//control should not come here ideally
								//update the seek to skip the bad logged data read
								dwLastseek = dwLastseek + iIdx;
							}
						} else {
							//file system issue TODO
						}
					}

					if ((fr == FR_OK) && pcTmp) {
						//read so far is successful and time stamp is found
						memset(SampleData, 0, sizeof(SampleData));
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
						strcat(SampleData, "IMEI=");
						if (g_pInfoA->cfgIMEI[0] != -1) {
							strcat(SampleData, g_pInfoA->cfgIMEI);
						} else {
							strcat(SampleData, DEF_IMEI);//be careful as devices with unprogrammed IMEI will need up using same DEF_IMEI
						}
						strcat(SampleData,
								"&ph=8455523642&v=1.20140817.1&sid=0|1|2|3|4&"); //SERIAL
#else
						strcat(SampleData,"IMEI=358072043113601&ph=8455523642&v=1.20140817.1&sid=0|1|2|3&"); //SERIAL
#endif
						//check if time stamp is split across the two sources
						iOffset = iPOSTstatus - (int) pcTmp; //reuse, iPOSTstatus is end of first src
						if ((iOffset > 0) && (iOffset < TS_SIZE)) {
							//reuse the SampleData tail part to store the complete timestamp
							pcData = &SampleData[SAMPLE_LEN - 1] - TS_SIZE - 1; //to prevent overwrite
							//memset(g_TmpSMScmdBuffer,0,SMS_CMD_LEN);
							//pcData = &g_TmpSMScmdBuffer[0];
							memcpy(pcData, pcTmp, iOffset);
							memcpy(&pcData[iOffset], pcSrc2, TS_SIZE - iOffset);
							pcTmp = pcData;	//point to the copied timestamp
						}

						//format the timestamp
						strcat(SampleData, "sdt=");
						strncat(SampleData, &pcTmp[4], 4);
						strcat(SampleData, "/");
						strncat(SampleData, &pcTmp[8], 2);
						strcat(SampleData, "/");
						strncat(SampleData, &pcTmp[10], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[13], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[16], 2);
						strcat(SampleData, ":");
						strncat(SampleData, &pcTmp[19], 2);
						strcat(SampleData, "&");

						//format the interval
						strcat(SampleData, "i=");
						iOffset = formatfield(pcSrc1, "R", iPOSTstatus, NULL, 0,
								pcSrc2, 7);
						if ((!iOffset) && (pcSrc2)) {
							//format SP from second source
							formatfield(pcSrc2, "R", iIdx, NULL, 0, NULL, 0);
						}
						strcat(SampleData, "&");

						//format the temperature
						strcat(SampleData, "t=");
						iOffset = formatfield(pcSrc1, "A", iPOSTstatus, NULL, 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "A", iIdx, NULL, iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "B", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "B", iIdx, "|", iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "C", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "C", iIdx, "|", iOffset, NULL,
									0);
						}
						iOffset = formatfield(pcSrc1, "D", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "D", iIdx, "|", iOffset, NULL,
									0);
						}
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
						iOffset = formatfield(pcSrc1, "E", iPOSTstatus, "|", 0,
								pcSrc2, 8);
						if (pcSrc2) {
							formatfield(pcSrc2, "E", iIdx, "|", iOffset, NULL,
									0);
						}
#endif
						strcat(SampleData, "&");

						strcat(SampleData, "b=");
						iOffset = formatfield(pcSrc1, "F", iPOSTstatus, NULL, 0,
								pcSrc2, 7);
						if (pcSrc2) {
							formatfield(pcSrc2, "F", iIdx, NULL, iOffset, NULL,
									0);
						}
						strcat(SampleData, "&");

						strcat(SampleData, "p=");
						iOffset = formatfield(pcSrc1, "P", iPOSTstatus, NULL, 0,
								pcSrc2, 5);
						if (pcSrc2) {
							formatfield(pcSrc2, "P", iIdx, NULL, iOffset, NULL,
									0);
						}

						//update seek for the next sample
						dwLastseek = dwFinalSeek;
#ifndef CALIBRATION
						if (!(iStatus & TEST_FLAG)) {
							iPOSTstatus = 1;//indicate data is available for POST & SMS
						} else {
							iPOSTstatus = 0;//indicate data is available for POST & SMS
						}
#else
						iPOSTstatus = 0; //no need to send data for POST & SMS for device under calibration
#endif
						g_pInfoB->dwLastSeek = dwLastseek;

						//check if catch is needed due to backlog
						if ((filr.fsize - dwLastseek) > SAMPLE_SIZE) {
							iStatus |= BACKLOG_UPLOAD_ON;
						} else {
							iStatus &= ~BACKLOG_UPLOAD_ON;
						}

					}
					f_close(&filr);
				}
#endif

				if (iPOSTstatus) {
					iPOSTstatus = 0;
					//initialize the RX counters as RX buffer is been used in the aggregrate variables for HTTP POST formation
					uart_resetbuffer();

					iPOSTstatus = dopost(SampleData);
					if (iPOSTstatus != 0) {
						//redo the post
						uart_tx(
								"AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n"); //APN
						//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
						delay(MODEM_TX_DELAY2);

						uart_tx("AT#SGACT=1,1\r\n");
						delay(MODEM_TX_DELAY2);

						uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
						delay(MODEM_TX_DELAY2);
#ifdef NO_CODE_SIZE_LIMIT
						iPOSTstatus = dopost(SampleData);
						if (iPOSTstatus != 0) {
							//iHTTPRetryFailed++;
							//trigger sms failover
							__no_operation();
						} else {
							//iHTTPRetrySucceeded++;
							__no_operation();
						}
#endif
					}
					//iTimeCnt = 0;
					uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
					delay(MODEM_TX_DELAY2);

					//if upload sms
					delay(5000);		//opt sleep to get http post response
					uploadsms();
					// added for sms retry and file pointer movement..//
					file_pointer_enabled_sms_status = dopost_sms_status();
					if ((file_pointer_enabled_sms_status)
							|| (file_pointer_enabled_gprs_status)) {
						__no_operation();
					} else {
						dwLastseek = dw_file_pointer_back_log;// file pointer moved to original position need to tested.//
						g_pInfoB->dwLastSeek = dwLastseek;
					}

#ifdef POWER_SAVING_ENABLED
					modem_enter_powersave_mode();
#endif
				}
				lcd_show(iDisplayId);//remove the custom print (e.g transmitting)
			}
			P4IE |= BIT1;					// enable interrupt for button input
		}

		if ((iMinuteTick - iSampleTimeElapsed) >= g_iSamplePeriod) {
			iSampleTimeElapsed = iMinuteTick;
			P4IE &= ~BIT1;				// disable interrupt for button input
			//lcd_print_line("Sampling........", LINE2);
			//re-trigger the ADC conversion
			//ADC12CTL0 &= ~ADC12ENC;
			//ADC12CTL0 |= ADC12ENC | ADC12SC;
			sampletemp();
			delay(2000);	//to allow the ADC conversion to complete

			if (iStatus & NETWORK_DOWN) {
				delay(2000);//additional delay to enable ADC conversion to complete
				//check for signal strength
				uart_resetbuffer();
				uart_tx("AT+CSQ\r\n");
				delay(MODEM_TX_DELAY1);
				memset(ATresponse, 0, sizeof(ATresponse));
				uart_rx(ATCMD_CSQ, ATresponse);
				if (ATresponse[0] != 0) {
					iSignalLevel = strtol(ATresponse, 0, 10);
				}

				if ((iSignalLevel > NETWORK_UP_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					//update the network state
					iStatus &= ~NETWORK_DOWN;
					//send heartbeat
					sendhb();
				}
			}

		}

#ifndef CALIBRATION
		if ((iMinuteTick - iSMSRxPollElapsed) >= SMS_RX_POLL_INTERVAL) {
			iSMSRxPollElapsed = iMinuteTick;
			lcd_print_line("Configuring...", LINE1);
			delay(100);

			//update the signal strength
			uart_tx("AT+CSQ\r\n");
			delay(2000);
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CSQ, ATresponse);

			if (ATresponse[0] != 0) {
				iSignalLevel = strtol(ATresponse, 0, 10);
			}

			signal_gprs = dopost_gprs_connection_status(GPRS);

			iIdx = 0;
			while (iIdx < MODEM_CHECK_RETRY) {
				gprs_network_indication = dopost_gprs_connection_status(GSM);
				if((gprs_network_indication == 0)
						|| ((iSignalLevel < NETWORK_DOWN_SS)
								|| (iSignalLevel > NETWORK_MAX_SS))) {
					lcd_print_line("Signal lost...", LINE2);
					delay(100);
					iStatus |= NETWORK_DOWN;
					iOffset = 0;
					modem_init(g_pInfoA->cfgSIMSlot);
					lcd_print_line("Reconnecting...", LINE2);
					delay(100);
					iIdx++;
				} else {
					iStatus &= ~NETWORK_DOWN;
					break;
				}
			}

			if (iIdx == MODEM_CHECK_RETRY) {
				//switch the SIM slot;
				if (g_pInfoA->cfgSIMSlot != 2) { //value will be 0xFF in case FRAM was not already populated
					//current sim slot is 1
					//change to sim slot 2
					g_pInfoA->cfgSIMSlot = 2;
					lcd_print_line("Switching SIM: 2", LINE2);
					delay(100);
				} else {
					//current sim slot is 2
					//change to sim slot 1
					g_pInfoA->cfgSIMSlot = 1;
					lcd_print_line("Switching SIM: 1", LINE2);
					delay(100);
				}

				modem_init(g_pInfoA->cfgSIMSlot);
			}

#if 1
			//sms config reception and processing
			dohttpsetup();
			memset(ATresponse, 0, CFG_SIZE);
			doget(ATresponse);
			if (ATresponse[0] == '$') {
				if (processmsg(ATresponse)) {
					//send heartbeat on successful processing of SMS message
					sendhb();
				}
			} else {
				//no cfg message recevied
				//check signal strength
				iOffset = -1; //reuse to indicate no cfg msg was received
			}
			deactivatehttp();
#endif
		}
#endif

#ifndef BUZZER_DISABLED
		if(iStatus & BUZZER_ON)
		{
			iIdx = 10;	//TODO fine-tune for 5 to 10 secs
			while((iIdx--) && (iStatus & BUZZER_ON))
			{
				P3OUT |= BIT4;
				delayus(1);
				P3OUT &= ~BIT4;
				delayus(1);
			}
		}
#endif

		if (((iMinuteTick - iLCDShowElapsed) >= LCD_REFRESH_INTERVAL)
				|| (iLastDisplayId != iDisplayId)) {
			iLastDisplayId = iDisplayId;
			iLCDShowElapsed = iMinuteTick;
			lcd_show(iDisplayId);
		}
		//iTimeCnt++;

#ifndef CALIBRATION
		//process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
		if ((iMinuteTick) && !(iStatus & BACKLOG_UPLOAD_ON)
				&& ((iMinuteTick - iSMSRxPollElapsed)
						< (SMS_RX_POLL_INTERVAL - 2))
				&& ((iMinuteTick - iUploadTimeElapsed) < (g_iUploadPeriod - 2))
				&& ((iMinuteTick - iMsgRxPollElapsed) >= MSG_REFRESH_INTERVAL)) {
			iMsgRxPollElapsed = iMinuteTick;
			//check if messages are available
			uart_resetbuffer();

			uart_tx("AT+CPMS?\r\n");
			delay(1000);
			memset(ATresponse, 0, sizeof(ATresponse));
			uart_rx(ATCMD_CPMS, ATresponse);
			if (ATresponse[0] != 0) {
				iIdx = strtol(ATresponse, 0, 10);
				if (iIdx) {
					iIdx = 1;
					lcd_print_line("Msg Processing..", LINE2);
					while (iIdx <= SMS_READ_MAX_MSG_IDX) {
						memset(ATresponse, 0, sizeof(ATresponse));
						recvmsg(iIdx, ATresponse);
						if (ATresponse[0] != 0)	//check for $
								{
							switch (ATresponse[0]) {
							case '1':
								//get temperature values
								memset(SampleData, 0, MSG_RESPONSE_LEN);
								for (iOffset = 0; iOffset < MAX_NUM_SENSORS;
										iOffset++) {
									strcat(SampleData, SensorName[iOffset]);
									strcat(SampleData, "=");
									strcat(SampleData, Temperature[iOffset]);
									strcat(SampleData, "C, ");
								}

								// added for show msg//
								strcat(SampleData, "Battery:");
								strcat(SampleData, itoa(iBatteryLevel));
								strcat(SampleData, "%, ");
								if (P4IN & BIT4)	//power not plugged
								{
									strcat(SampleData, "POWER OUT");
								} else if (((P4IN & BIT6))
										&& (iBatteryLevel == 100)) {
									strcat(SampleData, "FULL CHARGE");
								} else {
									strcat(SampleData, "CHARGING");
								}
								iOffset = strlen(SampleData);
								SampleData[iOffset] = 0x1A; //ctrl-Z
								memset(tmpstr, 0, 40); //reuse tmpstr and filler space to form CMGS
								strcat(tmpstr, "AT+CMGS=\"");
								strcat(tmpstr, &ATresponse[6]);
								strcat(tmpstr, "\",129\r\n");
								uart_tx(tmpstr);
								delay(5000);
								uart_tx(SampleData);
								delay(1000);

								break;

							case '2':
								//reset the board by issuing a SW BOR
								PMM_trigBOR();
								while (1);	//code should not come here

							default:
								break;
							}
							//if(processmsg(ATresponse))
							{
								//send heartbeat on successful processing of SMS message
								// sendhb();
							}
						}
						iIdx++;
					}
					delallmsg();
					lcd_show(iDisplayId);
				}
			}
		}

		//low power behavior
		if ((iBatteryLevel <= 10) && (P4IN & BIT4)) {
			lcd_clear();
			delay(1000);
			lcd_print("Low Battery     ");
			delay(5000);
			//reset display
			//PJOUT |= BIT6;							// LCD reset pulled high
			//disable backlight
			lcd_bldisable();
			lcd_off();

			//loop for charger pluging
			while (iBatteryLevel <= 10) {
				if (iLastDisplayId != iDisplayId) {
					iLastDisplayId = iDisplayId;
					//enable backlight
					lcd_blenable();
					//lcd reset
					//lcd_reset();
					//enable lcd
					//lcd_init();
					lcd_on();

					lcd_clear();
					lcd_print("Low Battery     ");
					delay(5000);
					//disable backlight
					lcd_bldisable();
					lcd_off();
					//reset lcd
					//PJOUT |= BIT6;							// LCD reset pulled high
				}

				//power plugged in
				if (!(P4IN & BIT4)) {
					lcd_blenable();
					lcd_on();
					lcd_print("Starting...");
					modem_init(g_pInfoA->cfgSIMSlot);
					iLastDisplayId = iDisplayId = 0;
					lcd_show(iDisplayId);
					break;
				}
			}

		}
#endif
		__no_operation();                       // SET BREAKPOINT HERE
	}
}

void writetoI2C(uint8_t addressI2C, uint8_t dataI2C) {
	return; //void function for the new board with LCD
/*
#ifndef I2C_DISABLED
	delay(100);
	i2c_write(SLAVE_ADDR_DISPLAY, addressI2C, 1, &dataI2C);
#endif
*/
}

void ConvertADCToTemperature(unsigned int ADCval, char* TemperatureVal,
		int8_t iSensorIdx) {
	float A0V2V, A0R2, A0R1 = 10000, A0V1 = 2.5;
	float A0tempdegC = 0.0;
	int32_t A0tempdegCint = 0;
	volatile int32_t A0R2int = 0;
	int8_t i = 0;
	int8_t iTempIdx = 0;

	A0V2V = 0.00061 * ADCval;		//Converting to voltage. 2.5/4096 = 0.00061

	//NUM = A0V2V*A0R1;
	//DEN = A0V1-A0V2V;

	A0R2 = (A0V2V * A0R1) / (A0V1 - A0V2V);				//R2= (V2*R1)/(V1-V2)

	A0R2int = A0R2;

	//Convert resistance to temperature using Steinhart-Hart algorithm
	A0tempdegC = ConvertoTemp(A0R2int);

	//use calibration formula
#ifndef CALIBRATION
	if ((g_pInfoB->calibration[iSensorIdx][0] > -2.0)
			&& (g_pInfoB->calibration[iSensorIdx][0] < 2.0)
			&& (g_pInfoB->calibration[iSensorIdx][1] > -2.0)
			&& (g_pInfoB->calibration[iSensorIdx][1] < 2.0)) {
		A0tempdegC = A0tempdegC * g_pInfoB->calibration[iSensorIdx][1]
				+ g_pInfoB->calibration[iSensorIdx][0];
	}
#endif
	//Round to one digit after decimal point
	A0tempdegCint = (int32_t) (A0tempdegC * 10);
	//A0tempdegC = A0tempdegC/100;

	if (A0tempdegCint < TEMP_CUTOFF) {
		TemperatureVal[0] = '-';
		TemperatureVal[1] = '-';
		TemperatureVal[2] = '.';
		TemperatureVal[3] = '-';
	} else {
		if (A0tempdegCint < 0) {
			iTempIdx = 1;
			TemperatureVal[0] = '-';
			A0tempdegCint = abs(A0tempdegCint);

		}

		for (i = 2; i >= 0; i--) {
			TemperatureVal[i + iTempIdx] = A0tempdegCint % 10 + 0x30;
			A0tempdegCint = A0tempdegCint / 10;

		}
		TemperatureVal[3 + iTempIdx] = TemperatureVal[2 + iTempIdx];
		//TemperatureVal[0] = (char)results[0];
		//TemperatureVal[1] = (char)results[1];
		TemperatureVal[2 + iTempIdx] = (char) '.';
	}

}

#if 0
//------------------------------------------------------------------------------
// The USCIAB0TX_ISR is structured such that it can be used to transmit any
// number of bytes by pre-loading TXByteCtr with the byte count. Also, TXData
// points to the next byte to transmit.
//------------------------------------------------------------------------------
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_B0_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(USCI_B0_VECTOR)))
#endif
void USCIB0_ISR(void)
{
	switch(__even_in_range(UCB0IV,0x1E))
	{
		case 0x00: break;                      // Vector 0: No interrupts break;
		case 0x02: break;
		case 0x04:
		//resend start if NACK
		EUSCI_B_I2C_masterSendStart(EUSCI_B0_BASE);
		break;// Vector 4: NACKIFG break;
		case 0x18:
		if(TXByteCtr)// Check TX byte counter
		{
			EUSCI_B_I2C_masterMultiByteSendNext(EUSCI_B0_BASE,
					TXData);
			TXByteCtr--;                        // Decrement TX byte counter
		}
		else
		{
			EUSCI_B_I2C_masterMultiByteSendStop(EUSCI_B0_BASE);

			__bic_SR_register_on_exit(CPUOFF);    // Exit LPM0
		}
		break;                                // Vector 26: TXIFG0 break;
		default: break;
	}
}
#endif

float ConvertoTemp(float R) {
	float A1 = 0.00335, B1 = 0.0002565, C1 = 0.0000026059, D1 = 0.00000006329,
			tempdegC;
	int R25 = 9965;

	tempdegC = 1
			/ (A1 + (B1 * log(R / R25)) + (C1 * pow((log(R / R25)), 2))
					+ (D1 * pow((log(R / R25)), 3)));

	tempdegC = tempdegC - 273.15;

	return (float) tempdegC;

}

void parsetime(char* pDatetime, struct tm* pTime) {
	char* pToken1 = NULL;
	char* pToken2 = NULL;
	char* pDelimiter = "\"/,:+-";

	if ((pDatetime) && (pTime)) {
		//string format "yy/MM/dd,hh:mm:ss�zz"
		pToken1 = strtok(pDatetime, pDelimiter);
		if (pToken1) {
			pToken2 = strtok(NULL, pDelimiter);		//skip the first :
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_year = atoi(pToken2) + 2000;	//to get absolute year value
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_mon = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_mday = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_hour = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_min = atoi(pToken2);
			pToken2 = strtok(NULL, pDelimiter);
			pTime->tm_sec = atoi(pToken2);

		}
	}
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG)) {
	case ADC12IV_NONE:
		break;        // Vector  0:  No interrupt
	case ADC12IV_ADC12OVIFG:
		break;        // Vector  2:  ADC12MEMx Overflow
	case ADC12IV_ADC12TOVIFG:
		break;        // Vector  4:  Conversion time overflow
	case ADC12IV_ADC12HIIFG:
		break;        // Vector  6:  ADC12BHI
	case ADC12IV_ADC12LOIFG:
		break;        // Vector  8:  ADC12BLO
	case ADC12IV_ADC12INIFG:
		break;        // Vector 10:  ADC12BIN
	case ADC12IV_ADC12IFG0:                 // Vector 12:  ADC12MEM0 Interrupt
		if (ADC12MEM0 >= 0x7ff)               // ADC12MEM0 = A1 > 0.5AVcc?
			P1OUT |= BIT0;                      // P1.0 = 1
		else
			P1OUT &= ~BIT0;                     // P1.0 = 0
		__bic_SR_register_on_exit(LPM0_bits); // Exit active CPU
		break;                                // Clear CPUOFF bit from 0(SR)
	case ADC12IV_ADC12IFG1:
		break;        // Vector 14:  ADC12MEM1
	case ADC12IV_ADC12IFG2:   		        // Vector 16:  ADC12MEM2
		//ADCvar[0] = ADC12MEM2;                     // Read conversion result
		ADCvar[0] += ADC12MEM2;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG3:   		        // Vector 18:  ADC12MEM3
		//ADCvar[1] = ADC12MEM3;                     // Read conversion result
		ADCvar[1] += ADC12MEM3;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG4:   		        // Vector 20:  ADC12MEM4
		//ADCvar[2] = ADC12MEM4;                     // Read conversion result
		ADCvar[2] += ADC12MEM4;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG5:   		        // Vector 22:  ADC12MEM5
		//ADCvar[3] = ADC12MEM5;                     // Read conversion result
		ADCvar[3] += ADC12MEM5;                     // Read conversion result
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 4
		isConversionDone = 1;
#endif
		break;
	case ADC12IV_ADC12IFG6:                 // Vector 24:  ADC12MEM6
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
		//ADCvar[4] = ADC12MEM6;                     // Read conversion result
		ADCvar[4] += ADC12MEM6;                     // Read conversion result
		isConversionDone = 1;
		iSamplesRead++;
#endif
		break;
	case ADC12IV_ADC12IFG7:
		break;        // Vector 26:  ADC12MEM7
	case ADC12IV_ADC12IFG8:
		break;        // Vector 28:  ADC12MEM8
	case ADC12IV_ADC12IFG9:
		break;        // Vector 30:  ADC12MEM9
	case ADC12IV_ADC12IFG10:
		break;        // Vector 32:  ADC12MEM10
	case ADC12IV_ADC12IFG11:
		break;        // Vector 34:  ADC12MEM11
	case ADC12IV_ADC12IFG12:
		break;        // Vector 36:  ADC12MEM12
	case ADC12IV_ADC12IFG13:
		break;        // Vector 38:  ADC12MEM13
	case ADC12IV_ADC12IFG14:
		break;        // Vector 40:  ADC12MEM14
	case ADC12IV_ADC12IFG15:
		break;        // Vector 42:  ADC12MEM15
	case ADC12IV_ADC12IFG16:
		break;        // Vector 44:  ADC12MEM16
	case ADC12IV_ADC12IFG17:
		break;        // Vector 46:  ADC12MEM17
	case ADC12IV_ADC12IFG18:
		break;        // Vector 48:  ADC12MEM18
	case ADC12IV_ADC12IFG19:
		break;        // Vector 50:  ADC12MEM19
	case ADC12IV_ADC12IFG20:
		break;        // Vector 52:  ADC12MEM20
	case ADC12IV_ADC12IFG21:
		break;        // Vector 54:  ADC12MEM21
	case ADC12IV_ADC12IFG22:
		break;        // Vector 56:  ADC12MEM22
	case ADC12IV_ADC12IFG23:
		break;        // Vector 58:  ADC12MEM23
	case ADC12IV_ADC12IFG24:
		break;        // Vector 60:  ADC12MEM24
	case ADC12IV_ADC12IFG25:
		break;        // Vector 62:  ADC12MEM25
	case ADC12IV_ADC12IFG26:
		break;        // Vector 64:  ADC12MEM26
	case ADC12IV_ADC12IFG27:
		break;        // Vector 66:  ADC12MEM27
	case ADC12IV_ADC12IFG28:
		break;        // Vector 68:  ADC12MEM28
	case ADC12IV_ADC12IFG29:
		break;        // Vector 70:  ADC12MEM29
	case ADC12IV_ADC12IFG30:
		break;        // Vector 72:  ADC12MEM30
	case ADC12IV_ADC12IFG31:
		break;        // Vector 74:  ADC12MEM31
	case ADC12IV_ADC12RDYIFG:
		break;        // Vector 76:  ADC12RDY
	default:
		break;
	}
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
	switch (__even_in_range(P2IV, P2IV_P2IFG7)) {
	case P2IV_NONE:
		break;
	case P2IV_P2IFG0:
		break;
	case P2IV_P2IFG1:
		break;
	case P2IV_P2IFG2:
		//P3OUT &= ~BIT4;                           // buzzer off
		iStatus &= ~BUZZER_ON;
		break;

	case P2IV_P2IFG3:
		break;
	case P2IV_P2IFG4:
		break;
	case P2IV_P2IFG5:
		break;
	case P2IV_P2IFG6:
		break;
	case P2IV_P2IFG7:
		break;
	default:
		break;
	}
}

// Port 4 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR
__interrupt void Port_4(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) Port_4 (void)
#else
#error Compiler not supported!
#endif
{
	switch (__even_in_range(P4IV, P4IV_P4IFG7)) {
	case P4IV_NONE:
		break;
	case P4IV_P4IFG0:
		break;
	case P4IV_P4IFG1:
		iDisplayId = (iDisplayId + 1) % MAX_DISPLAY_ID;
		break;
	case P4IV_P4IFG2:
		break;
	case P4IV_P4IFG3:
		break;
	case P4IV_P4IFG4:
		break;
	case P4IV_P4IFG5:
		break;
	case P4IV_P4IFG6:
		break;
	case P4IV_P4IFG7:
		break;
	default:
		break;
	}
}

int dopost(char* postdata) {
	int isHTTPResponseAvailable = 0;
	int iRetVal = -1;

	if (iStatus & TEST_FLAG)
		return iRetVal;
#ifndef SAMPLE_POST
	memset(ATresponse, 0, sizeof(ATresponse));
#ifdef NOTFROMFILE
	strcpy(ATresponse,"AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",");
#else
	strcpy(ATresponse, "AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v3/\",");
#endif
	strcat(ATresponse, itoa(strlen(postdata)));
	strcat(ATresponse, ",0\r\n");
	//uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
	//delay(5000);

	isHTTPResponseAvailable = 0;
	//uart_tx("AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",383,0\r\n");
	uart_tx(ATresponse);
	delay(10000);		//opt
	iHTTPRespDelayCnt = 0;
	while ((!isHTTPResponseAvailable)
			&& iHTTPRespDelayCnt <= HTTP_RESPONSE_RETRY) {
		delay(1000);
		iHTTPRespDelayCnt++;
		uart_rx(ATCMD_HTTPSND, ATresponse);
		if (ATresponse[0] == '>') {
			isHTTPResponseAvailable = 1;
		}
	}

	if (isHTTPResponseAvailable) {
		file_pointer_enabled_gprs_status = 1; // added for gprs status for file pointer movement///
		uart_tx(postdata);
#if 0
		//testing split http post
		memset(filler,0,sizeof(filler));
		memcpy(filler, postdata, 200);
		uart_tx(filler);
		delay(10000);
		memset(filler,0,sizeof(filler));
		memcpy(filler, &postdata[200], iLen - 200);
		uart_tx(filler);
#endif
		//delay(10000);
		iRetVal = 0;
		iPostSuccess++;
	} else {
		file_pointer_enabled_gprs_status = 0; // added for gprs status for file pointer movement..////
		iPostFail++;
	}
#else
	uart_tx("AT#HTTPCFG=1,\"67.205.14.22\",80,0,,,0,120,1\r\n");
	delay(5000);
	uart_tx("AT#HTTPSND=1,0,\"/post.php?dir=galcore&dump\",26,1\r\n");
	isHTTPResponseAvailable = 0;
	iIdx = 0;
	while ((!isHTTPResponseAvailable) && iIdx <= HTTP_RESPONSE_RETRY)
	{
		delay(1000);
		iIdx++;
		uart_rx(ATCMD_HTTPSND,ATresponse);
		if(ATresponse[0] == '>')
		{
			isHTTPResponseAvailable = 1;
		}
	}

	if(isHTTPResponseAvailable)
	{
		iRetVal = 1;
		uart_tx("imessy=martina&mary=johyyy");
		delay(10000);
	}
#endif
	return iRetVal;
}

int dopost_sms_status(void) {
	int l_file_pointer_enabled_sms_status = 0;
	int isHTTPResponseAvailable = 0;
	int i = 0, j = 0;

	if (iStatus & TEST_FLAG)
		return l_file_pointer_enabled_sms_status;
	iHTTPRespDelayCnt = 0;
	while ((!isHTTPResponseAvailable)
			&& iHTTPRespDelayCnt <= HTTP_RESPONSE_RETRY) {
		delay(1000);
		iHTTPRespDelayCnt++;
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C') && (RXBuffer[j + 1] == 'M')
					&& (RXBuffer[j + 2] == 'G') && (RXBuffer[j + 3] == 'S')
					&& (RXBuffer[j + 4] == ':')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
	}
	if (isHTTPResponseAvailable == 0) {
		l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
	}
	return l_file_pointer_enabled_sms_status;
}

int dopost_gprs_connection_status(char status) {
	int l_file_pointer_enabled_sms_status = 0;
	int isHTTPResponseAvailable = 0;
	int i = 0, j = 0;

	if (iStatus & TEST_FLAG)
		return l_file_pointer_enabled_sms_status;
	iHTTPRespDelayCnt = 0;
	if (status == GSM) {
		uart_tx("AT+CREG?\r\n");
		delay(MODEM_TX_DELAY1);
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C') && (RXBuffer[j + 1] == 'R')
					&& (RXBuffer[j + 2] == 'E') && (RXBuffer[j + 3] == 'G')
					&& (RXBuffer[j + 4] == ':') && (RXBuffer[j + 8] == '1')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
		if (isHTTPResponseAvailable == 0) {
			l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
		}
	}
	//////
	if (status == GPRS) {
		uart_tx("AT+CGREG?\r\n");
		delay(MODEM_TX_DELAY1);
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C') && (RXBuffer[j + 1] == 'G')
					&& (RXBuffer[j + 2] == 'R') && (RXBuffer[j + 3] == 'E')
					&& (RXBuffer[j + 4] == 'G') && (RXBuffer[j + 5] == ':')
					&& (RXBuffer[j + 7] == '0') && (RXBuffer[j + 8] == ',')
					&& (RXBuffer[j + 9] == '1')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
		if (isHTTPResponseAvailable == 0) {
			uart_tx("AT+CGATT=1\r\n");
			l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
		}
	}

	return l_file_pointer_enabled_sms_status;

}

FRESULT logsampletofile(FIL* fobj, int* tbw) {
	int bw = 0;	//bytes written
	char* fn = getCurrentFileName(&currTime);

	if (!(iStatus & TEST_FLAG)) {
		fr = f_open(fobj, fn, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
		if (fr == FR_OK) {
			if (fobj->fsize) {
				//append to the file
				f_lseek(fobj, fobj->fsize);
			}
		}

		if (iStatus & LOG_TIME_STAMP) {
			rtc_get(&currTime);

#if 1
			memset(acLogData, 0, sizeof(acLogData));
			strcat(acLogData, "$TS=");
			strcat(acLogData, itoa(currTime.tm_year));
			strcat(acLogData, itoa(currTime.tm_mon));
			strcat(acLogData, itoa(currTime.tm_mday));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_hour));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_min));
			strcat(acLogData, ":");
			strcat(acLogData, itoa(currTime.tm_sec));
			strcat(acLogData, ",");
			strcat(acLogData, "R");	//removed =
			strcat(acLogData, itoa(g_iSamplePeriod));
			strcat(acLogData, ",");
			strcat(acLogData, "\n");
			fr = f_write(fobj, acLogData, strlen(acLogData), (UINT *)&bw);
#else
			bw = f_printf(fobj,"$TS=%04d%02d%02d:%02d:%02d:%02d,R=%d,", currTime.tm_year, currTime.tm_mon, currTime.tm_mday,
					currTime.tm_hour, currTime.tm_min, currTime.tm_sec,g_iSamplePeriod);
#endif
			if (bw > 0) {
				*tbw += bw;
				iStatus &= ~LOG_TIME_STAMP;
			}
		}

		//get battery level
#ifndef BATTERY_DISABLED
		iBatteryLevel = batt_getlevel();
#else
		iBatteryLevel = 0;
#endif
		//log sample period, battery level, power plugged, temperature values
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
		//bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,E=%s,", iBatteryLevel,
		//			  !(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3],Temperature[4]);
		memset(acLogData, 0, sizeof(acLogData));
		strcat(acLogData, "F");
		strcat(acLogData, itoa(iBatteryLevel));
		strcat(acLogData, ",");
		strcat(acLogData, "P");
		if (P4IN & BIT4) {
			strcat(acLogData, "0,");
		} else {
			strcat(acLogData, "1,");
		}
		strcat(acLogData, "A");
		strcat(acLogData, Temperature[0]);
		strcat(acLogData, ",");
		strcat(acLogData, "B");
		strcat(acLogData, Temperature[1]);
		strcat(acLogData, ",");
		strcat(acLogData, "C");
		strcat(acLogData, Temperature[2]);
		strcat(acLogData, ",");
		strcat(acLogData, "D");
		strcat(acLogData, Temperature[3]);
		strcat(acLogData, ",");
		strcat(acLogData, "E");
		strcat(acLogData, Temperature[4]);
		strcat(acLogData, ",");
		strcat(acLogData, "\n");
		fr = f_write(fobj, acLogData, strlen(acLogData), (UINT *)&bw);
#else
		bw = f_printf(fobj,"F=%d,P=%d,A=%s,B=%s,C=%s,D=%s,", iBatteryLevel,
				!(P4IN & BIT4),Temperature[0],Temperature[1],Temperature[2],Temperature[3]);
#endif

		if (bw > 0) {
			*tbw += bw;
		}

		f_sync(fobj);
		return f_close(fobj);
	} else {
		return FR_OK;
	}
}

int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize) {
	char* pcTmp = NULL;
	char* pcExt = NULL;
	int16_t iSampleCnt = 0;
	int16_t ret = 0;
	int8_t iFlag = 0;

	//pcTmp = strstr(pcSrc,fieldstr);
	pcTmp = strchr(pcSrc, fieldstr[0]);
	iFlag = iFlagVal;
	while ((pcTmp && !lastoffset)
			|| (pcTmp && (char *) lastoffset && (pcTmp < (char *) lastoffset))) {
		iSampleCnt = lastoffset - (int) pcTmp;
		if ((iSampleCnt > 0) && (iSampleCnt < iFieldSize)) {
			//the field is splitted across
			//reuse the SampleData tail part to store the complete timestamp
			pcExt = &SampleData[SAMPLE_LEN - 1] - iFieldSize - 1;//to accommodate field pair and null termination
			//memset(g_TmpSMScmdBuffer,0,SMS_CMD_LEN);
			//pcExt = &g_TmpSMScmdBuffer[0];
			memset(pcExt, 0, iFieldSize + 1);
			memcpy(pcExt, pcTmp, iSampleCnt);
			memcpy(&pcExt[iSampleCnt], pcExtSrc, iFieldSize - iSampleCnt);
			pcTmp = pcExt;	//point to the copied field
		}

		iSampleCnt = 0;
		while (pcTmp[FORMAT_FIELD_OFF + iSampleCnt] != ',')	//TODO recheck whenever the log formatting changes
		{
			iSampleCnt++;
		}
		if (iFlag) {
			//non first instance
			strcat(SampleData, ",");
			strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
		} else {
			//first instance
			if (seperator) {
				strcat(SampleData, seperator);
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			} else {
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			}
		}
		//pcTmp = strstr(&pcTmp[3],fieldstr);
		pcTmp = strchr(&pcTmp[FORMAT_FIELD_OFF], fieldstr[0]);
		iFlag++;
		ret = 1;
	}

	return ret;
}

#if 1

void monitoralarm() {
	int8_t iCnt = 0;

	//iterate through sensors
	for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
		//get sensor value
		//temp = &Temperature[iCnt];

		//check sensor was plugged out
		if (Temperature[iCnt][1] == '-') {
			TEMP_ALARM_CLR(iCnt);
			g_iAlarmCnfCnt[iCnt] = 0;
			continue;
		}

		iTemp = strtod(&Temperature[iCnt][0], NULL);
		//iTemp = strtod("24.5",NULL);
		//check for low temp threshold
		if (iTemp < g_pInfoA->stTempAlertParams[iCnt].threshcold) {
			//check if alarm is set for low temp
			if (TEMP_ALARM_GET(iCnt) != LOW_TEMP_ALARM_ON) {
				//set it off incase it was earlier confirmed
				TEMP_ALARM_CLR(iCnt);
				//set alarm to low temp
				TEMP_ALARM_SET(iCnt, LOW_TEMP_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == LOW_TEMP_ALARM_ON) {
				//low temp alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= g_pInfoA->stTempAlertParams[iCnt].mincold) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
#ifndef ALERT_UPLOAD_DISABLED
					if(!(iStatus & BACKLOG_UPLOAD_ON))
					{
						iStatus |= ALERT_UPLOAD_ON;	//indicate to upload sampled data if backlog is not progress
					}
#endif
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms if not send already
					if(!(iStatus & SMSED_LOW_TEMP))
					{
						memset(SampleData,0,SMS_ENCODED_LEN);
						strcat(SampleData, "Alert Sensor ");
						strcat(SampleData, SensorName[iCnt]);
						strcat(SampleData,": Temp too LOW for ");
						strcat(SampleData,itoa(pstCfgInfoA->stTempAlertParams[iCnt].mincold));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately.");	//superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
						iLen = strlen(SampleData);
						SampleData[iLen] = (char)0x1A;

						sendmsg(SampleData);

						iStatus |= SMSED_LOW_TEMP;
					}
#endif

					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}
		} else if (iTemp > g_pInfoA->stTempAlertParams[iCnt].threshhot) {
			//check if alarm is set for high temp
			if (TEMP_ALARM_GET(iCnt) != HIGH_TEMP_ALARM_ON) {
				//set it off incase it was earlier confirmed
				TEMP_ALARM_CLR(iCnt);
				//set alarm to high temp
				TEMP_ALARM_SET(iCnt, HIGH_TEMP_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == HIGH_TEMP_ALARM_ON) {
				//high temp alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= g_pInfoA->stTempAlertParams[iCnt].minhot) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
#ifndef ALERT_UPLOAD_DISABLED
					if(!(iStatus & BACKLOG_UPLOAD_ON))
					{
						iStatus |= ALERT_UPLOAD_ON;	//indicate to upload sampled data if backlog is not progress
					}
#endif
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms if not send already
					if(!(iStatus & SMSED_HIGH_TEMP))
					{
						memset(SampleData,0,SMS_ENCODED_LEN);
						strcat(SampleData, "Alert Sensor ");
						strcat(SampleData, SensorName[iCnt]);
						strcat(SampleData,": Temp too HIGH for ");
						strcat(SampleData,itoa(pstCfgInfoA->stTempAlertParams[iCnt].minhot));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately."); //superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
						iLen = strlen(SampleData);
						SampleData[iLen] = (char)0x1A;

						sendmsg(SampleData);

						iStatus |= SMSED_HIGH_TEMP;
					}
#endif
					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}

		} else {
			//check if alarm is set
			if (TEMP_ALARM_GET(iCnt) != TEMP_ALERT_OFF) {
				//reset the alarm
				TEMP_ALARM_CLR(iCnt);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			}
		}
	}

	//check for battery alert 
	iCnt = MAX_NUM_SENSORS;
	if (iBatteryLevel < g_pInfoA->stBattPowerAlertParam.battthreshold) {
		//check if battery alarm is set
		if (TEMP_ALARM_GET(iCnt) != BATT_ALARM_ON) {
			TEMP_ALARM_CLR(iCnt);//reset the alarm in case it was earlier confirmed
			//set battery alarm
			TEMP_ALARM_SET(iCnt, BATT_ALARM_ON);
			//initialize confirmation counter
			g_iAlarmCnfCnt[iCnt] = 0;
		} else if (TEMP_ALARM_GET(iCnt) == BATT_ALARM_ON) {
			//power alarm is already set, increment the counter
			g_iAlarmCnfCnt[iCnt]++;
			if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
					>= g_pInfoA->stBattPowerAlertParam.minutesbathresh) {
				TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
				//set buzzer if not set
				if (!(iStatus & BUZZER_ON)) {
					//P3OUT |= BIT4;
					iStatus |= BUZZER_ON;
				}
#ifdef SMS_ALERT
				//send sms LOW Battery: ColdTrace has now 15% battery left. Charge your device immediately.
				memset(SampleData,0,SMS_ENCODED_LEN);
				strcat(SampleData, "LOW Battery: ColdTrace has now ");
				strcat(SampleData,itoa(iBatteryLevel));
				strcat(SampleData, "battery left. Charge your device immediately.");
				iLen = strlen(SampleData);
				SampleData[iLen] = (char)0x1A;

				sendmsg(SampleData);
#endif

				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;

			}
		}
	} else {
		//reset battery alarm 
		TEMP_ALARM_SET(iCnt, BATT_ALERT_OFF);
		g_iAlarmCnfCnt[iCnt] = 0;
	}

	//check for power alert enable
	if (g_pInfoA->stBattPowerAlertParam.enablepoweralert) {
		iCnt = MAX_NUM_SENSORS + 1;
		if (P4IN & BIT4) {
			//check if power alarm is set
			if (TEMP_ALARM_GET(iCnt) != POWER_ALARM_ON) {
				TEMP_ALARM_CLR(iCnt);//reset the alarm in case it was earlier confirmed
				//set power alarm
				TEMP_ALARM_SET(iCnt, POWER_ALARM_ON);
				//initialize confirmation counter
				g_iAlarmCnfCnt[iCnt] = 0;
			} else if (TEMP_ALARM_GET(iCnt) == POWER_ALARM_ON) {
				//power alarm is already set, increment the counter
				g_iAlarmCnfCnt[iCnt]++;
				if ((g_iAlarmCnfCnt[iCnt] * g_iSamplePeriod)
						>= g_pInfoA->stBattPowerAlertParam.minutespower) {
					TEMP_ALARM_SET(iCnt, TEMP_ALERT_CNF);
					//set buzzer if not set
					if (!(iStatus & BUZZER_ON)) {
						//P3OUT |= BIT4;
						iStatus |= BUZZER_ON;
					}
#ifdef SMS_ALERT
					//send sms Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.
					memset(SampleData,0,SMS_ENCODED_LEN);
					strcat(SampleData, "Power Outage: ATTENTION! Power is out in your clinic. Monitor the fridge temperature closely.");
					iLen = strlen(SampleData);
					SampleData[iLen] = (char)0x1A;

					sendmsg(SampleData);
#endif
					//initialize confirmation counter
					g_iAlarmCnfCnt[iCnt] = 0;

				}
			}
		} else {
			//reset power alarm	
			TEMP_ALARM_SET(iCnt, POWER_ALERT_OFF);
			g_iAlarmCnfCnt[iCnt] = 0;
		}
	}

	//disable buzzer if it was ON and all alerts are gone
	if ((iStatus & BUZZER_ON) && (g_iAlarmStatus == 0)) {
		iStatus &= ~BUZZER_ON;
	}

}

void validatealarmthreshold() {

	int8_t iCnt = 0;

	for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
		//validate low temp threshold
		if ((g_pInfoA->stTempAlertParams[iCnt].threshcold < MIN_TEMP)
				|| (g_pInfoA->stTempAlertParams[iCnt].threshcold > MAX_TEMP)) {
			g_pInfoA->stTempAlertParams[iCnt].threshcold =
			LOW_TEMP_THRESHOLD;
		}

		if ((g_pInfoA->stTempAlertParams[iCnt].mincold
				< MIN_CNF_TEMP_THRESHOLD)
				|| (g_pInfoA->stTempAlertParams[iCnt].mincold
						> MAX_CNF_TEMP_THRESHOLD)) {
			g_pInfoA->stTempAlertParams[iCnt].mincold =
			ALARM_LOW_TEMP_PERIOD;
		}

		//validate high temp threshold
		if ((g_pInfoA->stTempAlertParams[iCnt].threshhot < MIN_TEMP)
				|| (g_pInfoA->stTempAlertParams[iCnt].threshhot > MAX_TEMP)) {
			g_pInfoA->stTempAlertParams[iCnt].threshhot =
			HIGH_TEMP_THRESHOLD;
		}
		if ((g_pInfoA->stTempAlertParams[iCnt].minhot
				< MIN_CNF_TEMP_THRESHOLD)
				|| (g_pInfoA->stTempAlertParams[iCnt].minhot
						> MAX_CNF_TEMP_THRESHOLD)) {
			g_pInfoA->stTempAlertParams[iCnt].minhot =
			ALARM_HIGH_TEMP_PERIOD;
		}

	}

}
#endif

void uploadsms() {
	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	int iIdx = 0;
	int iPOSTstatus = 0;
	char sensorId[2] = { 0, 0 };

	iPOSTstatus = strlen(SampleData);

	memset(sensorId, 0, sizeof(sensorId));
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
		//pcSrc1 is t=23.1|24.1|25.1|26.1|28.1
		pcTmp = strtok(&pcSrc1[2], "|&");
		for (iIdx = 0; (iIdx < MAX_NUM_SENSORS) && (pcTmp); iIdx++) {
			sensorId[0] = iIdx + 0x30;
			strcat(ATresponse, sensorId);	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc2 = &ATresponse[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc2);
				strcat(ATresponse, pcSrc2);	//add encoded temp value
			} else {
				strcat(ATresponse, "/W");	//add encoded temp value
			}
			strcat(ATresponse, ",");

			pcTmp = strtok(NULL, "|&");

		}
	} else {
		iIdx = 0;
		pcSrc2 = strstr(pcSrc1, "|");	//end of first sensor
		if (!pcSrc2)
			pcSrc2 = pcData;
		pcTmp = strtok(&pcSrc1[2], ",|&");  //get first temp value

		while ((pcTmp) && (pcTmp < pcSrc2)) {
			sensorId[0] = iIdx + 0x30;
			strcat(ATresponse, sensorId);	//add sensor id
			strcat(ATresponse, ",");

			//encode the temp value
			memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc1);
				strcat(ATresponse, pcSrc1);	//add encoded temp value
			} else {
				strcat(ATresponse, "/W");	//add encoded temp value
			}

			pcTmp = strtok(NULL, ",|&");
			while ((pcTmp) && (pcTmp < pcSrc2)) {
				//encode the temp value
				memset(&ATresponse[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
				//check temp is --.- in case of sensor plugged out
				if (pcTmp[1] != '-') {
					pcSrc1 = &ATresponse[SMS_ENCODED_LEN]; //reuse
					encode(strtod(pcTmp, NULL), pcSrc1);
					strcat(ATresponse, pcSrc1);	//add encoded temp value
				} else {
					strcat(ATresponse, "/W");	//add encoded temp value
				}
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
		//strcat(ATresponse,",");
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

	sendmsg(ATresponse);

}

int8_t processmsg(char* pSMSmsg) {
	char* pcTmp = NULL;
	int8_t iCnt = 0;
	int8_t iCurrSeq = 0;

	pcTmp = strtok(pSMSmsg, ",");

	while ((pcTmp) && (iCurrSeq != -1)) {
		if (pcTmp) {
			//get msg type
			switch (pcTmp[3]) {
			case '1':
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					strncpy(g_pInfoA->cfgSMSCenter[g_pInfoA->cfgSIMSlot][0], pcTmp, strlen(pcTmp));

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
							iStatus |= RESET_ALERT;
							//set buzzer OFF
							iStatus &= ~BUZZER_ON;
							//reset alarm state and counters
							for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
								//reset the alarm
								TEMP_ALARM_CLR(iCnt);
								//initialize confirmation counter
								g_iAlarmCnfCnt[iCnt] = 0;
							}
						} else {
							iStatus &= ~RESET_ALERT;
						}
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							if (strtol(pcTmp, 0, 10)) {
								iStatus |= ENABLE_SECOND_SLOT;
								g_pInfoA->cfgSIMSlot = 2;
							} else {
								iStatus &= ~ENABLE_SECOND_SLOT;
								g_pInfoA->cfgSIMSlot = 1;
							}
							if (g_pInfoA->cfgSIMSlot
									!= g_pInfoA->cfgSIMSlot) {
								//sim slots need to be switched
								modem_init(g_pInfoA->cfgSIMSlot);
							}
						}
						iCnt = 1;
					}
				}
				break;
			case '2':
				pcTmp = strtok(NULL, ",");
#if 1
				if (pcTmp) {
					iCurrSeq = strtol(pcTmp, 0, 10);
					pcTmp = strtok(NULL, ",");
				}

				if (iCurrSeq != g_iLastCfgSeq) {
					g_iLastCfgSeq = iCurrSeq;
#else
					if(pcTmp)
					{
#endif
					for (iCnt = 0; (iCnt < MAX_NUM_SENSORS) && (pcTmp);
							iCnt++) {
						g_pInfoA->stTempAlertParams[iCnt].mincold = strtol(
								pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							g_pInfoA->stTempAlertParams[iCnt].threshcold =
									strtod(pcTmp, NULL);
							pcTmp = strtok(NULL, ",");
							if (pcTmp) {
								g_pInfoA->stTempAlertParams[iCnt].minhot =
										strtol(pcTmp, 0, 10);
								pcTmp = strtok(NULL, ",");
								if (pcTmp) {
									g_pInfoA->stTempAlertParams[iCnt].threshhot =
											strtod(pcTmp, NULL);
									pcTmp = strtok(NULL, ",");
								}
							}
						}
					}
					if (pcTmp) {
						g_pInfoA->stBattPowerAlertParam.minutespower =
								strtol(pcTmp, 0, 10);
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

					} else {
						g_pInfoA->stBattPowerAlertParam.enablepoweralert = 0;
						g_pInfoA->stBattPowerAlertParam.battthreshold = 101;
					}
					iCnt = 1;
				} else {
					iCurrSeq = -1; //stop further processing as cfg is identical
				}
				break;
			case '3':
				iCnt = 0;
#if 0
				//defunct SMS destination number configuration
				pcTmp = strtok(NULL,",");
				if(pcTmp)
				{
					g_pInfoB->iDAcount=0;
					for(iCnt; (iCnt < MAX_SMS_NUM) && (pcTmp); iCnt++)
					{
						memcpy(g_pInfoB->cfgSMSDA[iCnt],pcTmp,strlen(pcTmp));
						pcTmp = strtok(NULL,",");
					}
					g_pInfoB->iDAcount = iCnt;
				}
#endif
				break;
			case '4':
				break;
			}
			pcTmp = strtok(NULL, ","); //get next mesg
		}
	}
	return iCnt;
}

void sendhb() {

	char* pcTmp = NULL;

	lcd_print("HEART BEAT");
	//send heart beat
	memset(SampleData, 0, sizeof(SampleData));
	strcat(SampleData, SMS_HB_MSG_TYPE);
#if 0
	strcat(SampleData,"358072043113601");	//TODO remove hardcoding //SERIAL
	strcat(SampleData,",");
#else
	strcat(SampleData, g_pInfoA->cfgIMEI);
	strcat(SampleData, ",");
	if (g_pInfoA->cfgSIMSlot == 2) {
		strcat(SampleData, "1,");
	} else {
		strcat(SampleData, "0,");
	}
#endif
	//strcat(SampleData, g_pInfoA->cfgSMSC[0][g_pInfoA->cfgSIMSlot]); // Append the message service center so the backend cand send us back the right APN for it
	strcat(SampleData, "0");
	strcat(SampleData, ",");
#if MAX_NUM_SENSORS == 5
	strcat(SampleData, "1,1,1,1,1,");//TODO to be changed based on jack detection
#else
			strcat(SampleData,"1,1,1,1,");	//TODO to be changed based on jack detection
#endif

	pcTmp = itoa(batt_getlevel());	//opt by directly using tmpstr
	strcat(SampleData, pcTmp);
	if (P4IN & BIT4) {
		strcat(SampleData, ",0");
	} else {
		strcat(SampleData, ",1");
	}

	strcat(SampleData, ",(db)" __TIME__);

	strcat(SampleData, ctrlZ);
	sendmsg(SampleData);
}

void sampletemp() {
	int iIdx = 0;
	int iLastSamplesRead = 0;
	iSamplesRead = 0;

	//initialze ADCvar
	for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
		ADCvar[iIdx] = 0;
	}

	for (iIdx = 0; iIdx < SAMPLE_COUNT; iIdx++) {
		ADC12CTL0 &= ~ADC12ENC;
		ADC12CTL0 |= ADC12ENC | ADC12SC;
		while ((iSamplesRead - iLastSamplesRead) == 0);
		iLastSamplesRead = iSamplesRead;
//		delay(10);	//to allow the ADC conversion to complete
	}

	if ((isConversionDone) && (iSamplesRead > 0)) {
		for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
			ADCvar[iIdx] /= iSamplesRead;
		}
	}

}
