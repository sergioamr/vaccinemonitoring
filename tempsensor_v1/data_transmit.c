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
	strcat(data, itoa_nopadding(batt_getlevel()));
	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_isPlugged()));
	sms_send_message(data);
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

// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,
// sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.
void setup_http_post() {
	SIM_CARD_CONFIG *sim = config_getSIM();
	config_setLastCommand(COMMAND_POST);
	uart_resetbuffer();

	if (http_post(ATresponse) != 0) {
		//redo the post
		// Define Packet Data Protocol Context - +CGDCONT
		uart_txf("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0\r\n", sim->cfgAPN);
		uart_tx("AT#SGACT=1,1\r\n");
		uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
#ifdef NO_CODE_SIZE_LIMIT
		if (http_post(ATresponse) != 0) {
			// Try to upload via SMS instead
			uart_tx("AT#SGACT=1,0\r\n"); // deactivate GPRS context
			//if upload sms
			delay(5000); // opt sleep to get http post response
			data_upload_sms();
			//file_pointer_enabled_sms_status =  TODO clarify what this return is for
			http_post_sms_status();
		} else {
			__no_operation();
		}
#endif
	}

#ifdef POWER_SAVING_ENABLED
		modem_enter_powersave_mode();
#endif
	config_setLastCommand(COMMAND_POST + COMMAND_END);
}

// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
void process_batch() {
	int lineIndex = 0;
	uint8_t triggerTimestamp = 0;
	FILINFO fili;
	DIR dir;
	FIL filr;
	FRESULT fr;
	char line[80];
	char path[32];
	int lineSize = sizeof(line)/sizeof(char);
	char* dateString = NULL;
	char* format = "IMEI=%s&ph=%s&v=%s&sid=%s&sdt=%s&i=%s";
	const static char* defSID = "0|1|2|3|4";
	const static char* delim1 = "/";
	const static char* delim2 = ":";
	SIM_CARD_CONFIG *sim = config_getSIM();
	struct tm firstDate;

	memset(ATresponse, 0, sizeof(ATresponse));

	lcd_printl(LINE2, "Transmitting...");

	// Cycle through all files using f_findfirst, f_findnext.
	fr = f_findfirst(&dir, &fili, FOLDER_TEXT, "*." EXTENSION_TEXT);
	while(fr == FR_OK) {
		sprintf(path, "%s/%s", FOLDER_TEXT, fili.fname);
		fr = f_open(&filr, path, FA_READ | FA_OPEN_ALWAYS);
		// If we must carry on where we left off cycle through the lines
		// until we get to where we left off
		if (g_pSysCfg->lastLineRead > 0) {
			while(lineIndex < g_pSysCfg->lastLineRead) {
				if (f_gets(line, lineSize, &filr) == 0) {
					lineIndex = 0;
					break;
				}
				lineIndex++;
			}
			triggerTimestamp = 1;
		}

		//strcpy(line, f_gets(line, strlen(line), &filr));
		while(f_gets(line, lineSize, &filr) != 0) {
			dateString = strstr(line, "$TS");
			if(lineIndex == 0 || triggerTimestamp) {
				// What if this line isn't a date? -> Find previous date or next?
				parse_time_from_line(&firstDate, line);
				dateString = get_date_string(&firstDate, delim1, delim2, 0);
				sprintf(ATresponse, format,
						g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa",
						defSID, dateString,
						itoa_nopadding(g_pDevCfg->stIntervalParam.loggingInterval));
				triggerTimestamp = 0;
			} else {
				// Stream data! When it's streamed we should always be able to send
				// a whole block of data at a time (unless an error occurs)
				if(dateString == NULL) {
					strcat(ATresponse, line);
				} else {
					//setup_http_post();
					triggerTimestamp = 1;
					memset(ATresponse, 0, sizeof(ATresponse));
					// Done - Send!
					// Send the data if the last line has been passed or
					// the TXbuffer is full.
					// Found next time stamp - Move to next batch now
				}
			}

			g_pSysCfg->lastLineRead = lineIndex;
			lineIndex++;
		}

		if (f_close(&filr) == FR_OK) {
			fr = f_unlink(path); // Delete the file
			g_pSysCfg->lastLineRead = 0;
		} else {
			break; // Something broke, run away and try again later
			// TODO should really try to delete file again
		}

		if (f_findnext(&dir, &fili) != FR_OK) {
			break;
		}
	}

	lcd_printl(LINEC, "Transmit");
	lcd_printl(LINE2, "Done");
}

int data_transmit(uint8_t *pSampleCnt) {
	return 0;
}
