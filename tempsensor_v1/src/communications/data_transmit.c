/*
 * data_transmit.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "buzzer.h"

#define MAX_LINE_UPLOAD_TEXT 40

#define TRANS_FAILED		   -1
#define TRANS_SUCCESS			0

char *getSensorTemp(int sensorID) {
	static char sensorData[4];
	return sensorData;
}

uint8_t data_send_temperatures_sms() {
	char data[MAX_SMS_SIZE_FULL];
	int t = 0;

	rtc_getlocal(&g_tmCurrTime);

	strcpy(data, SMS_DATA_MSG_TYPE);
	strcat(data, get_simplified_date_string(&g_tmCurrTime));
	for (t = 0; t < SYSTEM_NUM_SENSORS; t++) {
		strcat(data, getSensorTemp(t));
	}

	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_getlevel()));
	strcat(data, ",");
	strcat(data, itoa_nopadding(batt_isPlugged()));
	return sms_send_message(data);
}

<<<<<<< HEAD
int8_t data_upload_sms(FIL *file, uint32_t start, uint32_t end) {
	char line[MAX_LINE_UPLOAD_TEXT], encodedLine[40];
=======
int8_t data_send_sms(FIL *file, uint32_t start, uint32_t end) {
	char line[80], encodedLine[40];
>>>>>>> Restructure of failover
	int lineSize = sizeof(line) / sizeof(char);
	char* dateString = NULL;
	struct tm firstDate;
	char smsMsg[MAX_SMS_SIZE];
	uint8_t splitSend = 0;
	uint16_t linesParsed = 0;
	uint8_t length = strlen(smsMsg);

	f_lseek(file, start);

	do {
		if (splitSend) {
			offset_timestamp(&firstDate, linesParsed);
			dateString = get_date_string(&firstDate, "", "", "", 0);
			sprintf(smsMsg, "%d,%s,%d,%d,", 11, dateString,
					g_pDevCfg->sIntervalsMins.sampling, 5);
			strcat(smsMsg, encodedLine);
			linesParsed = splitSend = 0;
		} else if (file->fptr == start) {
			// Must get first line before transmitting to calculate the length properly
			if (f_gets(line, lineSize, file) != 0) {
				parse_time_from_line(&firstDate, line);
				dateString = get_date_string(&firstDate, "", "", "", 0);
				sprintf(smsMsg, "%d,%s,%d,%d,", 11, dateString,
						g_pDevCfg->sIntervalsMins.sampling, 5);
			} else {
				return TRANS_FAILED;
			}
		} else {
			break; //done
		}

		length = strlen(smsMsg);

		// check that the transmitted data equals the size to send
		while (file->fptr < end) {
			if (f_gets(line, lineSize, file) != 0) {
				linesParsed++;
				replace_character(line, '\n', '\0');
				encode_string(line, encodedLine, ",");
				length += strlen(encodedLine);
				if (length > MAX_SMS_SIZE) {
					splitSend = 1;
					break;
				} else {
					strcat(smsMsg, encodedLine);
				}
			} else {
				return TRANS_FAILED;
			}
		}

		if (sms_send_message(smsMsg) != UART_SUCCESS) {
			return TRANS_FAILED;
		}
	} while (splitSend);

	return TRANS_SUCCESS;
}

// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,
// sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.
// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
<<<<<<< HEAD
int8_t http_send_batch(FIL *file, uint32_t start, uint32_t end) {
	int uart_state;
	char line[MAX_LINE_UPLOAD_TEXT];
=======
int8_t data_send_http(FIL *file, uint32_t start, uint32_t end) {
	char line[160];
>>>>>>> Restructure of failover
	int retry = 0;

	char* dateString = NULL;
	struct tm firstDate;

	SIM_CARD_CONFIG *sim = config_getSIM();

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, sizeof(line), file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(line, "IMEI=%s&ph=%s&v=%s&sdt=%s&i=%d&t=",
				g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa", dateString,
				g_pDevCfg->sIntervalsMins.sampling);
	} else {
		// This is not really a gprs problem but a file problem - new state required?
		return TRANS_FAILED;
	}

	uint32_t length = strlen(line) + (end - file->fptr);
	http_open_connection_upload(length);

	// Send the date line
	uart_tx_nowait(line);
	if (uart_getTransactionState()!=UART_SUCCESS) {
		return TRANS_FAILED;
	}

	lcd_progress_wait(400);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, sizeof(line), file) != 0) {
			if (file->fptr != end) {
				replace_character(line, '\n', '|');
				uart_tx_nowait(line);

				if (uart_getTransactionState() != UART_SUCCESS) {
					return TRANS_FAILED;
				}

				lcd_print_progress();
			} else {
				// Last line! Wait for OK
				// This command also disables the appending of AT and \r\n on the data
				uart_setHTTPDataMode();
				uart_tx_data(line, TIMEOUT_HTTPSND, 1); // We don't have more than one attempt to send data
				uart_setOKMode();
				if (uart_getTransactionState() != UART_SUCCESS) {
					http_check_error(&retry);
					return TRANS_FAILED;
				}
			}
		} else {
			return TRANS_FAILED;
		}
	}

	return TRANS_SUCCESS;
}

int8_t data_send_method(FIL *file, uint32_t start, uint32_t end) {
	if (g_pSysState->simState[g_pDevCfg->cfgSIM_slot].failsGPRS > 0 || state_isGSM()) {
		if (data_send_sms(file, start, end) != TRANS_SUCCESS) {
			state_failed_gsm(g_pDevCfg->cfgSIM_slot);
			return TRANS_FAILED;
		}
	} else {
		if (data_send_http(file, start, end) != TRANS_SUCCESS) {
			state_failed_gprs(g_pDevCfg->cfgSIM_slot);
			return TRANS_FAILED;
		}
	}
	return TRANS_SUCCESS;
}

void process_batch() {
	uint8_t canSend = 0, transactionState = 0;
	uint32_t seekFrom = g_pSysState->lastSeek, seekTo = g_pSysState->lastSeek;
	char line[MAX_LINE_UPLOAD_TEXT];
	char path[32];
	char do_not_process_batch = false;
	int lineSize = sizeof(line) / sizeof(char);

	FILINFO fili;
	DIR dir;
	FIL filr;
	FRESULT fr;
	int len;

	log_disable();

	if (!state_isSimOperational()) {
		return;
	}

	// Cycle through all files using f_findfirst, f_findnext.
	fr = f_findfirst(&dir, &fili, FOLDER_TEXT, "*." EXTENSION_TEXT);
	if (fr != FR_OK) {
		return;
	}

	if (g_pSysState->safeboot.disable.data_transmit) {
		do_not_process_batch = true; // TODO remove old corrupted file
	}

	g_pSysState->safeboot.disable.data_transmit = 1;

	while (fr == FR_OK) {
		sprintf(path, "%s/%s", FOLDER_TEXT, fili.fname);
		fr = f_open(&filr, path, FA_READ | FA_OPEN_ALWAYS);
		if (fr != FR_OK) {
			break;
		}

		// If the last file was corrupted and forced a reboot we remove the extension
		if (do_not_process_batch) {
			sprintf(line, "%s/%s", FOLDER_TEXT, fili.fname);
			len = strlen(line);
			line[len-3]=0;
			f_close(&filr);
			f_rename(path, line);
			http_deactivate();
			g_pSysState->safeboot.disable.data_transmit = 0;
			return;
		}

		lcd_printl(LINEC, "Transmitting...");
		lcd_printl(LINE2, fili.fname);

		if (g_pSysState->lastSeek > 0) {
			f_lseek(&filr, g_pSysState->lastSeek);
		}

		while (f_gets(line, lineSize, &filr) != 0) {
			if (filr.fptr == 0 || strstr(line, "$TS") != NULL) {
				if (canSend) {
					canSend = 0;
					transactionState = data_send_method(&filr, seekFrom, seekTo);
					if (transactionState != TRANS_SUCCESS) {
						break;
					}

					seekFrom = seekTo = filr.fptr;
					// Found next time stamp - Move to next batch now
				}
			} else {
				canSend = 1;
				seekTo = filr.fptr;
			}
		}

		if(canSend) {
			transactionState = data_send_method(&filr, seekFrom, seekTo);
			seekFrom = canSend = 0;
		}

		if (seekTo < filr.fsize && transactionState == TRANS_SUCCESS) {
			g_pSysState->lastSeek = filr.fptr;
		} else if (transactionState == TRANS_SUCCESS) {
			g_pSysState->lastSeek = 0;
		}

		if (f_close(&filr) == FR_OK && transactionState == TRANS_SUCCESS) {
			if (g_pSysState->lastSeek == 0) {
				fr = f_unlink(path); // Delete the file
			}
		} else {
			break; // Tranmission failed
		}

		fr = f_findnext(&dir, &fili);
		if (strlen(fili.fname) == 0) {
			break;
		}
	}

	if (state_isGPRS()) {
		http_deactivate();
	}

	lcd_printl(LINEC, "Transmission");
	if (transactionState == TRANS_SUCCESS) {
		lcd_printl(LINE2, "Complete");
	} else {
		lcd_printl(LINE2, "Failed");
	}

	g_pSysState->safeboot.disable.data_transmit = 0;
	g_iStatus |= LOG_TIME_STAMP; // Uploads may take a long time and might require offset to be reset
	log_enable();
}
