/*
 * data_transmit.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "config.h"
#include "fatdata.h"
#include "battery.h"
#include "stdlib.h"

#define TS_SIZE				21
#define TS_FIELD_OFFSET		1	//1 - $, 3 - $TS


// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.

char *getSensorTemp(int sensorID) {
	static char sensorData[64];

	return sensorData;
}

void data_send_temperatures_sms() {
	char data[180];
	int t=0;

	rtc_getlocal(&g_tmCurrTime);

	strcpy(data, SMS_DATA_MSG_TYPE);
	strcat(data, get_simplified_date_string(&g_tmCurrTime));
	for (t=0; t<MAX_NUM_SENSORS; t++) {
		strcat(data, getSensorTemp(t));
	}

	strcat(data, ",");
	strcat(data, itoa_nopadding(g_iBatteryLevel));
	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_isPlugged()));
	sms_send_message(data);
}

// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
void process_batch() {
	/*
	int lineIndex = 0;
	FILINFO fili;
	FIL filr;
	FRESULT fr;
	DIR dir;
	FIL filr;
	FRESULT fr;
	char line[64];
	char* firstDateString = NULL;
	char* defSID = NULL;
	SIM_CARD_CONFIG *sim = config_getSIM();
	struct tm firstDate, currentDate;

	if(MAX_NUM_SENSORS == 5) {
		defSID = "0|1|2|3|4";
	} else {
		defSID = "0|1|2|3";
	}

	lcd_printl(LINE2, "Transmitting...");

	// Cycle through all files using f_findfirst, f_findnext.
	fr = f_findfirst(&dir, &fili, "/txt", ".txt");
	while(fr == 0) {
		fr = f_open(&filr, fili.fname, FA_READ | FA_OPEN_ALWAYS);
		while(f_gets(line, strlen(line), &filr) == 0 && fr == 0) {
			if(lineIndex == 0) {
				parse_time_from_line(&firstDate, line);
				firstDateString = get_date_string(&firstDate, "/", ":", 0);
				sprintf(SampleData, "IMEI=%s&ph=%s&v=%s&sid=%s&sdt=%s&i=%lu",
						g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa",
						defSID, firstDateString,
						g_pDevCfg->stIntervalParam.loggingInterval);
			} else {
				strcat(SampleData, line);
				if(f_gets(line, strlen(line), &filr) == 0) {
					parse_time_from_line(&currentDate, line);
					if(!date_within_interval(&currentDate, &firstDate,
							g_pDevCfg->stIntervalParam.loggingInterval)) {
						break;
					}
				}
			}

			g_pSysCfg->lastLineRead = lineIndex;
			lineIndex++;
		}
		// Send the data if the last line has been passed or
		// the TXbuffer is full or the interval time rule was broken.
		// If it was the last line delete the file & set line number to 0,
		// otherwise save the line number (when TX buffer was full or the
		// interval rule was broken).
		fr = f_close(&filr);
		g_pSysCfg->lastLineRead = 0;
		fr = f_findnext(&dir, &fili);
	}
	*/
}

void data_upload_sms() {
/*
	int iPOSTstatus = 0;
	char* pcData = NULL;
	char* pcTmp = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;
	int iIdx = 0;
	char sensorId[2] = { 0, 0 };
	char dataSample[180];

	iPOSTstatus = strlen(SampleData);

	memset(sensorId, 0, sizeof(sensorId));
	memset(dataSample, 0, sizeof(dataSample));
	strcat(dataSample, SMS_DATA_MSG_TYPE);

	pcSrc1 = strstr(SampleData, "sdt=");	//start of TS
	pcSrc2 = strstr(pcSrc1, "&");	//end of TS
	pcTmp = strtok(&pcSrc1[4], "/:");

	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(dataSample, pcTmp);	//get year
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(dataSample, pcTmp);	//get month
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(dataSample, pcTmp);	//get day
				strcat(dataSample, ":");
			}
		}

		//fetch time
		pcTmp = strtok(NULL, "/:");
		if ((pcTmp) && (pcTmp < pcSrc2)) {
			strcat(dataSample, pcTmp);	//get hour
			pcTmp = strtok(NULL, "/:");
			if ((pcTmp) && (pcTmp < pcSrc2)) {
				strcat(dataSample, pcTmp);	//get minute
				pcTmp = strtok(NULL, "&");
				if ((pcTmp) && (pcTmp < pcSrc2)) {
					strcat(dataSample, pcTmp);	//get sec
					strcat(dataSample, ",");
				}
			}
		}
	}

	pcSrc1 = strstr(pcSrc2 + 1, "i=");	//start of interval
	pcSrc2 = strstr(pcSrc1, "&");	//end of interval
	pcTmp = strtok(&pcSrc1[2], "&");
	if ((pcTmp) && (pcTmp < pcSrc2)) {
		strcat(dataSample, pcTmp);
		strcat(dataSample, ",");
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
			strcat(dataSample, sensorId);	//add sensor id
			strcat(dataSample, ",");

			//encode the temp value
			memset(&dataSample[0], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc2 = &dataSample[0]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc2);
				strcat(dataSample, pcSrc2);	//add encoded temp value
			} else {
				strcat(dataSample, "/W");	//add encoded temp value
			}
			strcat(dataSample, ",");

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
			strcat(dataSample, sensorId);	//add sensor id
			strcat(dataSample, ",");

			//encode the temp value
			memset(&dataSample[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
			//check temp is --.- in case of sensor plugged out
			if (pcTmp[1] != '-') {
				pcSrc1 = &dataSample[SMS_ENCODED_LEN]; //reuse
				encode(strtod(pcTmp, NULL), pcSrc1);
				strcat(dataSample, pcSrc1);	//add encoded temp value
			} else {
				strcat(dataSample, "/W");	//add encoded temp value
			}

			pcTmp = strtok(NULL, ",|&");
			while ((pcTmp) && (pcTmp < pcSrc2)) {
				//encode the temp value
				memset(&dataSample[SMS_ENCODED_LEN], 0, ENCODED_TEMP_LEN);
				//check temp is --.- in case of sensor plugged out
				if (pcTmp[1] != '-') {
					pcSrc1 = &dataSample[SMS_ENCODED_LEN]; //reuse
					encode(strtod(pcTmp, NULL), pcSrc1);
					strcat(dataSample, pcSrc1);	//add encoded temp value
				} else {
					strcat(dataSample, "/W");	//add encoded temp value
				}
				pcTmp = strtok(NULL, ",|&");

			}

			//check if we can start with next temp series
			if ((pcTmp) && (pcTmp < pcData)) {
				strcat(dataSample, ",");
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
			strcat(dataSample, ",");
			pcSrc1 = strrchr(pcTmp, ','); //postion to the last battery level  (e.g pcTmp 100 or 100,100,..
			if (pcSrc1) {
				strcat(dataSample, pcSrc1 + 1); //past the last comma
			} else {
				strcat(dataSample, pcTmp); //no comma
			}
			strcat(dataSample, ",");

			pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
			if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
				strcat(dataSample, pcSrc1 + 1);
			} else {
				//power plugged does not have series values
				pcTmp = pcSrc2 + 1;
				strcat(dataSample, &pcTmp[2]); //pcTmp contains p=yyy
			}

		}

	} else {
		//battery does not have series values
		//strcat(dataSample,",");
		strcat(dataSample, &pcTmp[2]); //pcTmp contains b=yyy

		strcat(dataSample, ",");
		pcSrc2 = pcTmp + strlen(pcTmp);  //end of battery
		pcSrc1 = strrchr(pcSrc2 + 1, ','); //postion to the last power plugged state
		if ((pcSrc1) && (pcSrc1 < &SampleData[iPOSTstatus])) {
			strcat(dataSample, pcSrc1 + 1);	//last power plugged values is followed by null
		} else {
			//power plugged does not have series values
			pcTmp = pcSrc2 + 1;
			strcat(dataSample, &pcTmp[2]); //pcTmp contains p=yyy
		}

	}
	sms_send_message(dataSample);
	*/
}

int data_transmit(uint8_t *pSampleCnt) {
	/*
	int iPOSTstatus = 0;
	char *dummy = NULL;
	char* pcTmp = NULL;
	char* pcData = NULL;
	char* pcSrc1 = NULL;
	char* pcSrc2 = NULL;

	int32_t dwFinalSeek = 0;
	int32_t iSize = 0;
	int16_t iOffset = 0;
	int32_t dw_file_pointer_back_log = 0; // for error condition /// need to be tested.
	char file_pointer_enabled_sms_status = 0; // for sms condtition enabling.../// need to be tested
	int iIdx = 0;

	SIM_CARD_CONFIG *sim = config_getSIM();

	lcd_printl(LINE2, "Transmitting...");

	//iStatus &= ~TEST_FLAG;
#ifdef SMS_ALERT
	g_iStatus &= ~SMSED_HIGH_TEMP;
	g_iStatus &= ~SMSED_LOW_TEMP;
#endif
	if ((iMinuteTick - iUploadTimeElapsed) >= g_iUploadPeriod) {
		if((g_iUploadPeriod/g_iSamplePeriod) < MAX_NUM_CONTINOUS_SAMPLES){
			//trigger a new timestamp
			g_iStatus |= LOG_TIME_STAMP;
			*pSampleCnt = 0;
			//iStatus &= ~ALERT_UPLOAD_ON;	//reset alert upload indication
		}
		iUploadTimeElapsed = iMinuteTick;
	} else if ((g_iStatus & ALERT_UPLOAD_ON)
			&& (*pSampleCnt < MAX_NUM_CONTINOUS_SAMPLES)) {
		//trigger a new timestamp
		g_iStatus |= LOG_TIME_STAMP;
		*pSampleCnt = 0;
		g_iStatus &= ~ALERT_UPLOAD_ON;
	}

//reset the alert uplaod indication
	if (g_iStatus & ALERT_UPLOAD_ON) {
		g_iStatus &= ~ALERT_UPLOAD_ON;
	}

#ifdef POWER_SAVING_ENABLED
	modem_exit_powersave_mode();
#endif

	if (!(g_iStatus & TEST_FLAG)) {
		uart_tx("AT+CGDCONT=1,\"IP\",\"giffgaff.com\",\"0.0.0.0\",0,0\r\n"); //APN
		//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN
		uart_tx("AT#SGACT=1,1\r\n");
		uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
	}

#ifdef NOTFROMFILE
	iPOSTstatus = 0;	//set to 1 if post and sms should happen
	memset(SampleData,0,sizeof(SampleData));
	strcat(SampleData,"IMEI=358072043113601&phone=8455523642&uploadversion=1.20140817.1&sensorid=0|1|2|3&");//SERIAL
	rtc_get(&g_tmCurrTime);
	strcat(SampleData,"sampledatetime=");
	for(iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
	{
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_year)); strcat(SampleData,"/");
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_mon)); strcat(SampleData,"/");
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_mday)); strcat(SampleData,":");
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_hour)); strcat(SampleData,":");
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_min)); strcat(SampleData,":");
		strcat(SampleData,itoa_pad(g_tmCurrTime.tm_sec));
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
	char* fn = get_current_fileName(&g_tmCurrTime, FOLDER_TEXT, EXTENSION_TEXT);

//fr = f_read(&filr, acLogData, 1, &iIdx);   Read a chunk of source file
	memset(ATresponse, 0, sizeof(ATresponse));//ensure the buffer in aggregate_var section is more than AGGREGATE_SIZE
	fr = f_open(&filr, fn, FA_READ | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		dw_file_pointer_back_log = g_pCalibrationCfg->dwLastSeek; // added for dummy storing///
		//seek if offset is valid and not greater than existing size else read from the beginning
		if ((g_pCalibrationCfg->dwLastSeek != 0)
				&& (filr.fsize >= g_pCalibrationCfg->dwLastSeek)) {
			f_lseek(&filr, g_pCalibrationCfg->dwLastSeek);
		} else {
			g_pCalibrationCfg->dwLastSeek = 0;
		}

		iOffset = g_pCalibrationCfg->dwLastSeek % SECTOR_SIZE;
		//check the position is in first half of sector
		if ((SECTOR_SIZE - iOffset) > sizeof(ATresponse)) {
			fr = f_read(&filr, ATresponse, sizeof(ATresponse), (UINT *) &iIdx);  Read first chunk of sector
			if ((fr == FR_OK) && (iIdx > 0)) {
				g_iStatus &= ~SPLIT_TIME_STAMP;//clear the last status of splitted data
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
						g_iStatus &= ~SPLIT_TIME_STAMP; //all data in FATFS buffer
					} else {
						iPOSTstatus = &pcData[SECTOR_SIZE];	//mark first source end position as end of sector boundary
						g_iStatus |= SPLIT_TIME_STAMP;
					}
					pcTmp = iIdx;	//re-initialize to first time stamp

					//is all data within FATFS
					if (!(g_iStatus & SPLIT_TIME_STAMP)) {
						//first src is in FATFS buffer, second src is NULL
						pcSrc1 = pcTmp;
						pcSrc2 = NULL;
						//update lseek
						g_pCalibrationCfg->dwLastSeek += (iPOSTstatus - (int) pcSrc1);	//seek to the next time stamp
					}
					//check to read some more data
					else if ((filr.fsize - g_pCalibrationCfg->dwLastSeek)
							> (SECTOR_SIZE - iOffset)) {
						//update the seek to next sector
						g_pCalibrationCfg->dwLastSeek += SECTOR_SIZE - iOffset;
						//seek
						//f_lseek(&filr, g_pInfoB->dwLastSeek);
						//fr = f_read(&filr, ATresponse, AGGREGATE_SIZE, &iIdx);   Read next data of AGGREGATE_SIZE
						//if((fr == FR_OK) && (iIdx > 0))
						//if(disk_read_ex(0,ATresponse,filr.dsect+1,AGGREGATE_SIZE) == RES_OK)
						if (disk_read_ex(0, (BYTE *) ATresponse, filr.dsect + 1,
								512) == RES_OK) {
							//calculate bytes read
							iSize = filr.fsize - g_pCalibrationCfg->dwLastSeek;
							if (iSize >= sizeof(ATresponse)) {
								iIdx = sizeof(ATresponse);
							} else {
								iIdx = iSize;
							}
							//update final lseek for next sample
							//pcSrc1 = strstr(ATresponse,"$TS");
							pcSrc1 = strstr(ATresponse, "$");
							if (pcSrc1) {
								dwFinalSeek = g_pCalibrationCfg->dwLastSeek
										+ (pcSrc1 - ATresponse);//seek to the next TS
								iIdx = pcSrc1;		//second src end position
							}
							//no next time stamp found
							else {
								dwFinalSeek = g_pCalibrationCfg->dwLastSeek + iIdx;//update with bytes read
								g_pCalibrationCfg->dwLastSeek = &ATresponse[iIdx];//get postion of last byte
								iIdx = g_pCalibrationCfg->dwLastSeek;//end position for second src
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
						dwFinalSeek = filr.fsize;//EOF - update with file size

					}
				} else {
					//control should not come here ideally
					//update the seek to next sector to skip the bad logged data
					g_pCalibrationCfg->dwLastSeek += SECTOR_SIZE - iOffset;
				}
			} else {
				//file system issue TODO
			}
		} else {
			//the position is second half of sector
			fr = f_read(&filr, ATresponse, SECTOR_SIZE - iOffset, &iIdx);  Read data till the end of sector
			if ((fr == FR_OK) && (iIdx > 0)) {
				g_iStatus &= ~SPLIT_TIME_STAMP;//clear the last status of splitted data
				//get position of first time stamp
				//pcTmp = strstr(ATresponse,"$TS");
				pcTmp = strstr(ATresponse, "$");

				if ((pcTmp) && (pcTmp < &ATresponse[iIdx])) {
					//check there are enough bytes to check for second time stamp postion
					if (iIdx > TS_FIELD_OFFSET) {
						//check if all data is in ATresponse
						//pcSrc1 = strstr(&ATresponse[TS_FIELD_OFFSET],"$TS");
						pcSrc1 = strstr(&pcTmp[TS_FIELD_OFFSET], "$");
						if ((pcSrc1) && (pcSrc1 < &ATresponse[iIdx])) {
							iPOSTstatus = pcSrc1;	//first src end position;
							g_iStatus &= ~SPLIT_TIME_STAMP;//all data in FATFS buffer
						} else {
							iPOSTstatus = &ATresponse[iIdx]; //first src end position;
							g_iStatus |= SPLIT_TIME_STAMP;
						}
					} else {
						iPOSTstatus = &ATresponse[iIdx]; //first src end position;
						g_iStatus |= SPLIT_TIME_STAMP;
					}

					//check if data is splitted across
					if (g_iStatus & SPLIT_TIME_STAMP) {
						//check to read some more data
						if ((filr.fsize - g_pCalibrationCfg->dwLastSeek)
								> (SECTOR_SIZE - iOffset)) {
							//update the seek to next sector
							g_pCalibrationCfg->dwLastSeek += SECTOR_SIZE - iOffset;
							//seek
							f_lseek(&filr, g_pCalibrationCfg->dwLastSeek);
							fr = f_read(&filr, &dummy, 1, &iIdx);  dummy read to load the next sector
							if ((fr == FR_OK) && (iIdx > 0)) {
								pcData = (char *) FatFs.win; //resuse the buffer maintained by the file system
								//update final lseek for next sample
								//pcSrc1 = strstr(pcData,"$TS");
								pcSrc1 = strstr(pcData, "$");
								if ((pcSrc1)
										&& (pcSrc1 < &pcData[SECTOR_SIZE])) {
									g_pCalibrationCfg->dwLastSeek += (pcSrc1 - pcData);//seek to the next TS
									iIdx = pcSrc1;//end position for second src
								} else {
									dwFinalSeek = filr.fsize;//EOF - update with file size
									iIdx = &pcData[dwFinalSeek % SECTOR_SIZE];//end position for second src
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
							dwFinalSeek = filr.fsize;//EOF - update with file size
						}
					} else {
						//all data in ATresponse
						pcSrc1 = pcTmp;
						pcSrc2 = NULL;
						//update lseek
						g_pCalibrationCfg->dwLastSeek = g_pCalibrationCfg->dwLastSeek
								+ (iPOSTstatus - (int) pcSrc1);	//seek to the next time stamp
					}
				} else {
					//control should not come here ideally
					//update the seek to skip the bad logged data read
					g_pCalibrationCfg->dwLastSeek += iIdx;
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
			if (!check_address_empty(g_pDeviceCfg->cfgIMEI[0])) {
				strcat(SampleData, g_pDeviceCfg->cfgIMEI);
			} else {
				strcat(SampleData, DEF_IMEI);//be careful as devices with unprogrammed IMEI will need up using same DEF_IMEI
			}
			strcat(SampleData, "&ph=8455523642&v=1.20140817.1&sid=0|1|2|3|4&"); //SERIAL
#else
					strcat(SampleData,
							"IMEI=358072043113601&ph=8455523642&v=1.20140817.1&sid=0|1|2|3&"); //SERIAL
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
				pcTmp = pcData;			//point to the copied timestamp
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
			iOffset = formatfield(pcSrc1, "R", iPOSTstatus, NULL, 0, pcSrc2, 7);
			if ((!iOffset) && (pcSrc2)) {
				//format SP from second source
				formatfield(pcSrc2, "R", iIdx, NULL, 0, NULL, 0);
			}
			strcat(SampleData, "&");

			//format the temperature
			strcat(SampleData, "t=");
			iOffset = formatfield(pcSrc1, "A", iPOSTstatus, NULL, 0, pcSrc2, 8);
			if (pcSrc2) {
				formatfield(pcSrc2, "A", iIdx, NULL, iOffset, NULL, 0);
			}
			iOffset = formatfield(pcSrc1, "B", iPOSTstatus, "|", 0, pcSrc2, 8);
			if (pcSrc2) {
				formatfield(pcSrc2, "B", iIdx, "|", iOffset, NULL, 0);
			}
			iOffset = formatfield(pcSrc1, "C", iPOSTstatus, "|", 0, pcSrc2, 8);
			if (pcSrc2) {
				formatfield(pcSrc2, "C", iIdx, "|", iOffset, NULL, 0);
			}
			iOffset = formatfield(pcSrc1, "D", iPOSTstatus, "|", 0, pcSrc2, 8);
			if (pcSrc2) {
				formatfield(pcSrc2, "D", iIdx, "|", iOffset, NULL, 0);
			}
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
			iOffset = formatfield(pcSrc1, "E", iPOSTstatus, "|", 0, pcSrc2, 8);
			if (pcSrc2) {
				formatfield(pcSrc2, "E", iIdx, "|", iOffset, NULL, 0);
			}
#endif
			strcat(SampleData, "&");

			strcat(SampleData, "b=");
			iOffset = formatfield(pcSrc1, "F", iPOSTstatus, NULL, 0, pcSrc2, 7);
			if (pcSrc2) {
				formatfield(pcSrc2, "F", iIdx, NULL, iOffset, NULL, 0);
			}
			strcat(SampleData, "&");

			strcat(SampleData, "p=");
			iOffset = formatfield(pcSrc1, "P", iPOSTstatus, NULL, 0, pcSrc2, 5);
			if (pcSrc2) {
				formatfield(pcSrc2, "P", iIdx, NULL, iOffset, NULL, 0);
			}

			//update seek for the next sample
			g_pCalibrationCfg->dwLastSeek = dwFinalSeek;
#ifndef CALIBRATION
			if (!(g_iStatus & TEST_FLAG)) {
				iPOSTstatus = 1;	//indicate data is available for POST & SMS
			} else {
				iPOSTstatus = 0;	//indicate data is available for POST & SMS
			}
#else
			iPOSTstatus = 0; //no need to send data for POST & SMS for device under calibration
#endif

			//check if catch is needed due to backlog
			if ((filr.fsize - g_pCalibrationCfg->dwLastSeek) > sizeof(SampleData)) {
				g_iStatus |= BACKLOG_UPLOAD_ON;
			} else {
				g_iStatus &= ~BACKLOG_UPLOAD_ON;
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

		iPOSTstatus = http_post(SampleData);
		if (iPOSTstatus != 0) {
			//redo the post
			// Define Packet Data Protocol Context - +CGDCONT
			 uart_txf("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0\r\n", sim->cfgAPN);
			//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n"); //APN

			uart_tx("AT#SGACT=1,1\r\n");
			uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
#ifdef NO_CODE_SIZE_LIMIT
			iPOSTstatus = http_post(SampleData);
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
		uart_tx("AT#SGACT=1,0\r\n");			//deactivate GPRS context

		//if upload sms
		delay(5000);			//opt sleep to get http post response
		data_upload_sms();
		// added for sms retry and file pointer movement..//
		file_pointer_enabled_sms_status = http_post_sms_status();
		if ((file_pointer_enabled_sms_status)
				|| (file_pointer_enabled_gprs_status)) {
			__no_operation();
		} else {
			g_pCalibrationCfg->dwLastSeek = dw_file_pointer_back_log;// file pointer moved to original position need to tested.//
		}

#ifdef POWER_SAVING_ENABLED
		modem_enter_powersave_mode();
#endif
		config_setLastCommand(COMMAND_POST + COMMAND_END);
	}
*/
	return 0;
}
