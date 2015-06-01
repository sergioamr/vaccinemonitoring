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

#define MAX_NUM_CONTINOUS_SAMPLES 10

#define TS_SIZE				21
#define TS_FIELD_OFFSET		1	//1 - $, 3 - $TS

#include "thermalcanyon.h"

volatile char isConversionDone = 0;
volatile uint8_t iDisplayId = 0;
volatile int iSamplesRead = 0;

_Sigfun * signal(int i, _Sigfun *proc) {
	__no_operation();
	return NULL;
}

// Setup Pinout for I2C, and SPI transactions.
// http://www.ti.com/lit/ug/slau535a/slau535a.pdf

void setupClock_IO() {
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
}

static void setupADC_IO() {

	P1SELC |= BIT0 | BIT1;                    // Enable VEREF-,+
	P1SELC |= BIT2;                           // Enable A/D channel A2
#ifdef SEQUENCE
	P1SELC |= BIT3 | BIT4 | BIT5;          // Enable A/D channel A3-A5
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	P4SELC |= BIT2;          				// Enable A/D channel A10
#endif
#endif

#ifdef POWER_SAVING_ENABLED
	P3DIR |= BIT3;							// Modem DTR
	P3OUT &= ~BIT3;// DTR ON (low)
#endif
	//P1SEL0 |= BIT2;

	// Configure ADC12
	ADC12CTL0 = ADC12ON | ADC12SHT0_2;       // Turn on ADC12, set sampling time
	ADC12CTL1 = ADC12SHP;                     // Use pulse mode
	ADC12CTL2 = ADC12RES_2;                   // 12bit resolution
	ADC12CTL3 |= ADC12CSTARTADD_2;			// A2 start channel
	ADC12MCTL2 = ADC12VRSEL_4 | ADC12INCH_2; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 2
#ifdef SEQUENCE
	ADC12MCTL3 = ADC12VRSEL_4 | ADC12INCH_3; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 3
	ADC12MCTL4 = ADC12VRSEL_4 | ADC12INCH_4; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 4
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
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
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5 | ADC12IE6; // Enable ADC conv complete interrupt
#else
			ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5; // Enable ADC conv complete interrupt
#endif

}

static void setupIO() {

	// Configure GPIO
	P2DIR |= BIT3;							// SPI CS
	P2OUT |= BIT3;					// drive SPI CS high to deactive the chip
	P2SEL1 |= BIT4 | BIT5 | BIT6;             // enable SPI CLK, SIMO, SOMI
	PJSEL0 |= BIT4 | BIT5;                    // For XT1
	P1SEL1 |= BIT6 | BIT7;					// Enable I2C SDA and CLK

	uart_setIO();

	P4IE |= BIT1;							// enable interrupt for button input
	P2IE |= BIT2;							// enable interrupt for buzzer off
	P3DIR |= BIT4;      						// Set P3.4 buzzer output

	lcd_setupIO();

	P2IE |= BIT2;							// enable interrupt for buzzer off

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	setupClock_IO();
	uart_setClock();
	setupADC_IO();

#ifndef I2C_DISABLED
	//i2c_init(EUSCI_B_I2C_SET_DATA_RATE_400KBPS);
	i2c_init(380000);
#endif

	__bis_SR_register(GIE);		//enable interrupt globally
}

/****************************************************************************/
/*  MAIN                                                                    */
/****************************************************************************/

int main(void) {

	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	uint32_t iIdx = 0;
	int32_t iPOSTstatus = 0;
	int32_t dwLastseek = 0;
	int32_t dwFinalSeek = 0;
	int32_t iSize = 0;
	int16_t iOffset = 0;
	int8_t dummy = 0;
	int8_t iSampleCnt = 0;
	char gprs_network_indication = 0;
	int32_t dw_file_pointer_back_log = 0; // for error condition /// need to be tested.
	char file_pointer_enabled_sms_status = 0; // for sms condtition enabling.../// need to be tested

	WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT

	setupIO();
	config_init();// Checks if this system has been initialized. Reflashes config and runs calibration in case of being first flashed.

	config_setLastCommand(COMMAND_BOOT);

	lcd_reset();
	lcd_blenable();

#ifndef BATTERY_DISABLED
	batt_init();
#endif

	lcd_init();

	g_iLCDVerbose = VERBOSE_BOOTING;         // Booting is not completed
	lcd_print_ext(LINE1, "Booting %d",
			(int) g_pSysCfg->numberConfigurationRuns);

#ifndef _DEBUG
	lcd_print_lne(LINE2, g_pSysCfg->firmwareVersion); // Show the firmware version
	delay(5000);
#else
	lcd_print_lne(LINE2, "(db)" __TIME__);
#endif

	fat_init_drive();

	sampletemp();
#ifndef _DEBUG
	// to allow conversion to get over and prevent any side-effects to other interface like modem
	// TODO is this delay to help on the following bug from texas instruments ? (http://www.ti.com/lit/er/slaz627b/slaz627b.pdf)

	lcd_progress_wait(2000);
#endif

	P4OUT &= ~BIT0;                           // Reset high

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
		modem_init();
		modem_getExtraInfo();
		modem_pull_time();
		modem_checkSignal();
		modem_getSMSCenter();

#ifdef POWER_SAVING_ENABLED
		uart_tx("AT+CFUN=5\r\n");//full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);

		uart_tx("AT+CFUN?\r\n");//full modem functionality with power saving enabled (triggered via DTR)
		delay(MODEM_TX_DELAY1);
#endif

		lcd_print("Checking GPRS");
		/// added for gprs connection..//
		signal_gprs = dopost_gprs_connection_status(GPRS);
		gprs_network_indication = dopost_gprs_connection_status(GSM);

		// Reading the Service Center Address to use as message gateway
		// http://www.developershome.com/sms/cscaCommand.asp
		// Get service center address; format "+CSCA: address,address_type
		//uart_resetbuffer();
		// Disable echo from modem
		uart_tx("ATE0\r\n");
		//heartbeat
		for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
			memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);
			ConvertADCToTemperature(ADCvar[iIdx], &Temperature[iIdx][0], iIdx);
		}
		sms_send_heart_beat();
	}

	// Battery checks
	lcd_clear();
	lcd_print("Battery check");

#ifndef BATTERY_DISABLED
	iBatteryLevel = batt_getlevel();
#else
	iBatteryLevel = 0;
#endif

	if (iBatteryLevel == 0)
		lcd_print_lne(LINE2, "Battery FAIL");
	else if (iBatteryLevel > 100)
		lcd_print_lne(LINE2, "Battery UNKNOWN");
	else if (iBatteryLevel > 99)
		lcd_print_lne(LINE2, "Battery FULL");
	else if (iBatteryLevel > 15)
		lcd_print_lne(LINE2, "Battery OK");
	else if (iBatteryLevel)
		lcd_print_lne(LINE2, "Battery LOW");

	if (iBatteryLevel<15 || iBatteryLevel>100)
		delay(10000); // Delay to display that there is a state to show

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
	lcd_disable_verbose();

	lcd_print("Finished Boot");
	log_append("Finished Boot");

	while (1) {
		//check if conversion is complete
		if ((isConversionDone) || (iStatus & TEST_FLAG)) {
			//convert the current sensor ADC value to temperature
			for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
				memset(&Temperature[iIdx], 0, TEMP_DATA_LEN + 1);
				ConvertADCToTemperature(ADCvar[iIdx], &Temperature[iIdx][0],
						iIdx);
			}

#ifndef  NOTFROMFILE
			//log to file
			fr = log_sample_to_disk((int *) &iBytesLogged);
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
			sendmsg(MSGData);
#endif

			isConversionDone = 0;

			//if(iIdx >= SAMPLE_PERIOD)
			//if(((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) || (iStatus & TEST_FLAG) || (iStatus & ALERT_UPLOAD_ON))
			if ((((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod)
					|| (iStatus & TEST_FLAG) || (iStatus & BACKLOG_UPLOAD_ON)
					|| (iStatus & ALERT_UPLOAD_ON))
					&& !(iStatus & NETWORK_DOWN)) {

				lcd_print_lne(LINE2, "Transmitting....");
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
					uart_tx(
							"AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n"); //APN
					//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
					uart_tx("AT#SGACT=1,1\r\n");
					uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
				}

#ifdef NOTFROMFILE
				iPOSTstatus = 0;	//set to 1 if post and sms should happen
				memset(SampleData,0,sizeof(SampleData));
				strcat(SampleData,"IMEI=358072043113601&phone=8455523642&uploadversion=1.20140817.1&sensorid=0|1|2|3&");//SERIAL
				rtc_get(&currTime);
				strcat(SampleData,"sampledatetime=");
				for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
				{
					strcat(SampleData,itoa_pad(currTime.tm_year)); strcat(SampleData,"/");
					strcat(SampleData,itoa_pad(currTime.tm_mon)); strcat(SampleData,"/");
					strcat(SampleData,itoa_pad(currTime.tm_mday)); strcat(SampleData,":");
					strcat(SampleData,itoa_pad(currTime.tm_hour)); strcat(SampleData,":");
					strcat(SampleData,itoa_pad(currTime.tm_min)); strcat(SampleData,":");
					strcat(SampleData,itoa_pad(currTime.tm_sec));
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

				pcData = itoa_pad(batt_getlevel());;
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
				char* fn = get_current_fileName(&currTime);

				//fr = f_read(&filr, acLogData, 1, &iIdx);  /* Read a chunk of source file */
				memset(ATresponse, 0, sizeof(ATresponse)); //ensure the buffer in aggregate_var section is more than AGGREGATE_SIZE
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
					if ((SECTOR_SIZE - iOffset) > sizeof(ATresponse)) {
						fr = f_read(&filr, ATresponse, sizeof(ATresponse),
								(UINT *) &iIdx); /* Read first chunk of sector*/
						if ((fr == FR_OK) && (iIdx > 0)) {
							iStatus &= ~SPLIT_TIME_STAMP;//clear the last status of splitted data
							pcData = (char *) FatFs.win;//reuse the buffer maintained by the file system
							//check for first time stamp
							//pcTmp = strstr(&pcData[iOffset],"$TS");
							pcTmp = strstr(&pcData[iOffset], "$");//to prevent $TS rollover case
							if ((pcTmp) && (pcTmp < &pcData[SECTOR_SIZE])) {
								iIdx = (uint32_t) pcTmp; //start position
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
									if (disk_read_ex(0, (BYTE *) ATresponse,
											filr.dsect + 1, 512) == RES_OK) {
										//calculate bytes read
										iSize = filr.fsize - dwLastseek;
										if (iSize >= sizeof(ATresponse)) {
											iIdx = sizeof(ATresponse);
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
											pcData = (char *) FatFs.win;//resuse the buffer maintained by the file system
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
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
						strcat(SampleData, "IMEI=");
						if (g_pInfoA->cfgIMEI[0] != 0xFF) {
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
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
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
						if ((filr.fsize - dwLastseek) > sizeof(SampleData)) {
							iStatus |= BACKLOG_UPLOAD_ON;
						} else {
							iStatus &= ~BACKLOG_UPLOAD_ON;
						}

					}
					f_close(&filr);
				}
#endif

				if (iPOSTstatus) {

					config_setLastCommand(COMMAND_POST);
					iPOSTstatus = 0;
					//initialize the RX counters as RX buffer is been used in the aggregrate variables for HTTP POST formation
					uart_resetbuffer();

					iPOSTstatus = dopost(SampleData);
					if (iPOSTstatus != 0) {
						//redo the post
						// Define Packet Data Protocol Context - +CGDCONT
						sprintf(g_szTemp,
								"AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0\r\n",
								g_pInfoA->cfgAPN[g_pInfoA->cfgSIMSlot]);
						uart_tx(g_szTemp);
						//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN

						uart_tx("AT#SGACT=1,1\r\n");
						uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
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
					config_setLastCommand(COMMAND_POST + COMMAND_END);
				}
				lcd_show(iDisplayId);//remove the custom print (e.g transmitting)
			}
			P4IE |= BIT1;					// enable interrupt for button input
		}

		if ((iMinuteTick - iSampleTimeElapsed) >= g_iSamplePeriod) {
			iSampleTimeElapsed = iMinuteTick;
			P4IE &= ~BIT1;				// disable interrupt for button input
			//lcd_print_lne(LINE2, "Sampling........");
			//re-trigger the ADC conversion
			//ADC12CTL0 &= ~ADC12ENC;
			//ADC12CTL0 |= ADC12ENC | ADC12SC;
			sampletemp();
			delay(2000);	//to allow the ADC conversion to complete

			if (iStatus & NETWORK_DOWN) {
				delay(2000);//additional delay to enable ADC conversion to complete
				//check for signal strength

				modem_pull_time();
				modem_checkSignal();
				if ((iSignalLevel > NETWORK_DOWN_SS)
						&& (iSignalLevel < NETWORK_MAX_SS)) {
					//send heartbeat
					sms_send_heart_beat();
				}
			}

		}

#ifndef CALIBRATION
		if ((iMinuteTick - iSMSRxPollElapsed) >= SMS_RX_POLL_INTERVAL) {
			iSMSRxPollElapsed = iMinuteTick;
			lcd_clear();
			lcd_print_lne(LINE1, "Configuring...");
			delay(100);

			modem_checkSignal();
			signal_gprs = dopost_gprs_connection_status(GPRS);

			iIdx = 0;
			while (iIdx < MODEM_CHECK_RETRY) {
				gprs_network_indication = dopost_gprs_connection_status(GSM);
				if ((gprs_network_indication == 0)
						|| ((iSignalLevel < NETWORK_DOWN_SS)
								|| (iSignalLevel > NETWORK_MAX_SS))) {
					lcd_print_lne(LINE2, "Signal lost...");
					iStatus |= NETWORK_DOWN;
					iOffset = 0;
					modem_init();
					lcd_print_lne(LINE2, "Reconnecting...");
					iIdx++;
				} else {
					iStatus &= ~NETWORK_DOWN;
					break;
				}
			}

			if (iIdx == MODEM_CHECK_RETRY) {
				modem_swapSIM();
			}

			//sms config reception and processing
			dohttpsetup();
			memset(ATresponse, 0, sizeof(ATresponse));
			doget(ATresponse);
			if (ATresponse[0] == '$') {
				if (sms_process_msg(ATresponse)) {
					//send heartbeat on successful processing of SMS message
					sms_send_heart_beat();
				}
			} else {
				//no cfg message recevied
				//check signal strength
				iOffset = -1; //reuse to indicate no cfg msg was received
			}
			deactivatehttp();
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

			sms_process_messages(iMinuteTick, iDisplayId);
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
					modem_init();
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

void ConvertADCToTemperature(int32_t ADCval, char* TemperatureVal,
		int8_t iSensorIdx) {
	float A0V2V, A0R2;
	float A0tempdegC = 0.0;
	int32_t A0tempdegCint = 0;
	int8_t i = 0;
	int8_t iTempIdx = 0;

	A0V2V = 0.00061 * ADCval;		//Converting to voltage. 2.5/4096 = 0.00061

	//NUM = A0V2V*A0R1;
	//DEN = A0V1-A0V2V;

	A0R2 = (A0V2V * 10000.0) / (2.5 - A0V2V);			//R2= (V2*R1)/(V1-V2)

	//Convert resistance to temperature using Steinhart-Hart algorithm
	A0tempdegC = ConvertoTemp(A0R2);

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

float ConvertoTemp(float R) {
	float A1 = 0.00335, B1 = 0.0002565, C1 = 0.0000026059, D1 = 0.00000006329,
			tempdegC;
	float R25 = 9965.0;

	tempdegC = 1
			/ (A1 + (B1 * log(R / R25)) + (C1 * pow((log(R / R25)), 2))
					+ (D1 * pow((log(R / R25)), 3)));

	tempdegC = tempdegC - 273.15;

	return tempdegC;

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
		ADCvar[0] += ADC12MEM2;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG3:   		        // Vector 18:  ADC12MEM3
		ADCvar[1] += ADC12MEM3;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG4:   		        // Vector 20:  ADC12MEM4
		ADCvar[2] += ADC12MEM4;                     // Read conversion result
		break;
	case ADC12IV_ADC12IFG5:   		        // Vector 22:  ADC12MEM5
		ADCvar[3] += ADC12MEM5;                     // Read conversion result
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 4
				isConversionDone = 1;
#endif
		break;
	case ADC12IV_ADC12IFG6:                 // Vector 24:  ADC12MEM6
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
		ADCvar[4] += ADC12MEM6;                     // Read conversion result
		isConversionDone = 1;
		iSamplesRead++;
#endif
		break;
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
	default:
		break;
	}
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
						strcat(SampleData,itoa_pad(g_pInfoA->stTempAlertParams[iCnt].mincold));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately.");	//superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
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
						strcat(SampleData,itoa_pad(g_pInfoA->stTempAlertParams[iCnt].minhot));
						strcat(SampleData," minutes. Current Temp is ");
						strcat(SampleData,Temperature[iCnt]);
						//strcat(SampleData,"�C. Take ACTION immediately."); //superscript causes ERROR on sending SMS
						strcat(SampleData,"degC. Take ACTION immediately.");
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
				strcat(SampleData,itoa_pad(iBatteryLevel));
				strcat(SampleData, "battery left. Charge your device immediately.");
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

		if ((g_pInfoA->stTempAlertParams[iCnt].mincold < MIN_CNF_TEMP_THRESHOLD)
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
		if ((g_pInfoA->stTempAlertParams[iCnt].minhot < MIN_CNF_TEMP_THRESHOLD)
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
	sendmsg(ATresponse);
}

void sampletemp() {
	int iIdx = 0;
	int iLastSamplesRead = 0;
	iSamplesRead = 0;

	config_setLastCommand(COMMAND_TEMPERATURE_SAMPLE);

	//initialze ADCvar
	for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
		ADCvar[iIdx] = 0;
	}

	for (iIdx = 0; iIdx < SAMPLE_COUNT; iIdx++) {
		ADC12CTL0 &= ~ADC12ENC;
		ADC12CTL0 |= ADC12ENC | ADC12SC;
		while ((iSamplesRead - iLastSamplesRead) == 0)
			;
		iLastSamplesRead = iSamplesRead;
//		delay(10);	//to allow the ADC conversion to complete
	}

	if ((isConversionDone) && (iSamplesRead > 0)) {
		for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++) {
			ADCvar[iIdx] /= iSamplesRead;
		}
	}

}
