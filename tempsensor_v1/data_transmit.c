/*
 * data_transmit.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"

#define TS_SIZE				21
#define TS_FIELD_OFFSET		1	//1 - $, 3 - $TS

char *getSensorTemp(int sensorID) {
	static char sensorData[4];
	return sensorData;
}

void data_send_temperatures_sms() {
	char data[MAX_SMS_SIZE_FULL];
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

void data_upload_sms(FIL *file, uint32_t start, uint32_t end) {
	char line[80];
	int lineSize = sizeof(line)/sizeof(char);
	char* dateString = NULL;
	struct tm firstDate;
	char smsMsg[MAX_SMS_SIZE];

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(smsMsg, "%d,%s,%s,%d,",
				11, dateString, itoa_nopadding(g_pDevCfg->stIntervalParam.loggingInterval), 5);
	} else {
		return;
	}

	uint8_t length = strlen(smsMsg);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, lineSize, file) != 0) {
			length += strlen(line);
			if (length >= MAX_SMS_SIZE) {
				// dont send?
				break;
				/*
				length = length - smsMaxNoExtra;
				line[smsMaxNoExtra - length] = '\0';
				*/
			}

			if (file->fptr != end) {
				replace_character(line, '\n', ',');
				strcat(smsMsg, line);
			} else {
				// Last line! Wait for OK
				strcat(smsMsg, line);
			}
		} else {
			break;
		}
	}

	sms_send_message(smsMsg);
}



// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,
// sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.
// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
void http_send_batch(FIL *file, uint32_t start, uint32_t end) {
	int uart_state;
	int retry = 0;
	char line[80];
	int lineSize = sizeof(line)/sizeof(char);
	char* dateString = NULL;
	struct tm firstDate;

	SIM_CARD_CONFIG *sim = config_getSIM();

	// Setup connection
	if (modem_check_network() != UART_SUCCESS) {
		return;
	}

	if (http_enable() != UART_SUCCESS) {
		http_deactivate();
		data_upload_sms(file, start, end);
		return;
	}

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(line, "IMEI=%s&ph=%s&v=%s&sid=%s&sdt=%s&i=%d&t=",
				g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa",
				"0|1|2|3|4", dateString,
				g_pDevCfg->stIntervalParam.loggingInterval);
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
				uart_state = uart_getTransactionState();
				if (uart_state == UART_ERROR)
					http_check_error(&retry);  // Tries again
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
		lcd_printl(LINE2, fili.fname);

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
			seekFrom = canSend = 0;
		}

		if (f_close(&filr) == FR_OK) {
			// TODO If EOF was not reached and error occured do not delete file or reset lastSeek
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
