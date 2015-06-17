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
	static char sensorData[4];
	return sensorData;
}

void data_send_temperatures_sms() {
	char data[SMS_MAX_SIZE];
	int t = 0;

	rtc_getlocal(&g_tmCurrTime);

	strcpy(data, SMS_DATA_MSG_TYPE);
	strcat(data, get_simplified_date_string(&g_tmCurrTime));
	for (t = 0; t < MAX_NUM_SENSORS; t++) {
		//strcat(data, getSensorTemp(t));
	}

	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_getlevel()));
	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_isPlugged()));
	sms_send_message(data);
}

void data_upload_sms() {

	// TODO Implement
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
// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
void http_send_batch(FIL *file, uint32_t start, uint32_t end) {
	char* dateString = NULL;
	const static char* format = "IMEI=%s&ph=%s&v=%s&sid=%s&sdt=%s&i=%s";
	const static char* defSID = "0|1|2|3|4";
	const static char* delim = "";
	SIM_CARD_CONFIG *sim = config_getSIM();
	struct tm firstDate;
	char line[80];
	int lineSize = sizeof(line)/sizeof(char);

	// Setup connection
	if (modem_check_network() != UART_SUCCESS) {
		return;
	}

	if (http_enable() != UART_SUCCESS) {
		// TODO try to send sms
		http_deactivate();
		return;
	}

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, delim, delim, delim, 0);
		sprintf(line, format,
				g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa",
				defSID, dateString,
				itoa_nopadding(g_pDevCfg->stIntervalParam.loggingInterval));
	} else {
		return;
	}

	uint32_t length = strlen(line) + (end - file->fptr);
	http_open_connection(length);

	// Send the date line
	uart_tx_nowait(line);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, lineSize, file) != 0) {
			if (file->fptr != end) {
				replace_character(line, '\n', '|');
				uart_tx_nowait(line);
			} else {
				// Last line! Wait for OK
				uart_tx(line);
			}
		} else {
			break;
		}
	}

	http_deactivate();
}

void process_batch() {
	uint8_t canSend = 0;
	uint32_t seekFrom = g_pSysCfg->lastSeek, seekTo = g_pSysCfg->lastSeek;
	char line[80];
	char path[32];
	int lineSize = sizeof(line) / sizeof(char);

	//config_setLastCommand(COMMAND_POST);

	FILINFO fili;
	DIR dir;
	FIL filr;
	FRESULT fr;

	// Cycle through all files using f_findfirst, f_findnext.
	fr = f_findfirst(&dir, &fili, FOLDER_TEXT, "*." EXTENSION_TEXT);
	if (fr != FR_OK) {
		return;
	}

	while (fr == FR_OK) {
		sprintf(path, "%s/%s", FOLDER_TEXT, fili.fname);
		fr = f_open(&filr, path, FA_READ | FA_OPEN_ALWAYS);
		if (fr != FR_OK) {
			break;
		}

		lcd_printl(LINEC, "Transmitting...");
		lcd_printl(LINE2, path);

		if (g_pSysCfg->lastSeek > 0) {
			f_lseek(&filr, g_pSysCfg->lastSeek);
		}

		while(f_gets(line, lineSize, &filr) != 0) {
			if(filr.fptr == 0 || strstr(line, "$TS") != NULL) {
				if(canSend) {
					http_send_batch(&filr, seekFrom, seekTo);
					seekFrom = seekTo = filr.fptr;
					canSend = 0;
					// Found next time stamp - Move to next batch now
				} else {
					// TODO: what if it broke not on a date (rollback?)
				}
			} else {
				canSend = 1;
				seekTo = filr.fptr;
			}
		}

		if(canSend) {
			http_send_batch(&filr, seekFrom, filr.fptr);
			seekFrom = 0;
			canSend = 0;
			// Found next time stamp - Move to next batch now
		}

		if (f_close(&filr) == FR_OK) {
			fr = f_unlink(path); // Delete the file
			g_pSysCfg->lastSeek = 0;
			g_iStatus |= LOG_TIME_STAMP;
		} else {
			break; // Something broke, run away and try again later
			// TODO should really try to delete file again?
		}

		fr = f_findnext(&dir, &fili);
	}

	lcd_printl(LINEC, "Transmit");
	lcd_printl(LINE2, "Done");
}
