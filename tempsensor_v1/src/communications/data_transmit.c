/*
 * data_transmit.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "buzzer.h"

#define TRANS_FAILED		   -1
#define TRANS_SUCCESS			0

char *getSensorTemp(int sensorID) {
	static char sensorData[4];
	return sensorData;
}

uint8_t data_send_temperatures_sms() {

	config_setLastCommand(COMMAND_SEND_TEMPERATURES_SMS);

	char *data=getSMSBufferHelper();
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

int8_t data_send_sms(FIL *file, uint32_t start, uint32_t end) {
	uint16_t lineSize = 0;
	struct tm firstDate;

	char encodedLine[MAX_ENCODED_LINE_SIZE];
	char *line = getStringBufferHelper(&lineSize);
	char *smsMsg = getSMSBufferHelper();

	char* dateString = NULL;
	uint8_t splitSend = 0;
	uint16_t linesParsed = 0;
	uint8_t length = 0;

	int res = TRANS_FAILED;

	config_setLastCommand(COMMAND_SEND_DATA_SMS);

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
			} else
				goto release;

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
			} else
				goto release;
		}

		if (sms_send_message(smsMsg) != UART_SUCCESS)
			goto release;

	} while (splitSend);

	res = TRANS_SUCCESS;
	release:
		releaseStringBufferHelper();
	return res;
}

// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,
// sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.
// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
int8_t data_send_http(FIL *file, uint32_t start, uint32_t end) {
	int uart_state;
	uint16_t lineSize = 0;
	char *line = getStringBufferHelper(&lineSize);
	int retry = 0;
	uint32_t length = 0;
	struct tm firstDate;

	int res = TRANS_FAILED;
	char* dateString = NULL;

	SIM_CARD_CONFIG *sim = config_getSIM();

	f_lseek(file, start);

	config_setLastCommand(COMMAND_SEND_DATA_HTTP);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(line, "IMEI=%s&ph=%s&v=%s&sdt=%s&i=%d&t=", g_pDevCfg->cfgIMEI,
				sim->cfgPhoneNum, "0.1pa", dateString,
				g_pDevCfg->sIntervalsMins.sampling);
	} else {
		goto release;
	}

	length = strlen(line) + (end - file->fptr);
	http_open_connection_upload(length);

	// Send the date line
	uart_tx_nowait(line);
	if (uart_getTransactionState() != UART_SUCCESS) {
		goto release;
	}

	lcd_progress_wait(300);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, lineSize, file) != 0) {
			if (file->fptr != end) {
				replace_character(line, '\n', '|');
				uart_tx_nowait(line);
				if (uart_getTransactionState() != UART_SUCCESS) {
					goto release;
				}
				lcd_print_progress();
			} else {
				// Last line! Wait for OK

				// This command also disables the appending of AT and \r\n on the data
				uart_setHTTPDataMode();
				uart_tx_data(line, TIMEOUT_HTTPSND, 1); // We don't have more than one attempt to send data
				uart_state = uart_getTransactionState();
				uart_setOKMode();
				http_check_error(&retry);
				if (sim->http_last_status_code
						!= 200|| uart_state == UART_FAILED) {
					goto release;
				}
			}
		} else {
			goto release;
		}
	}

	res = TRANS_SUCCESS;

	// EXIT
	release:
		releaseStringBufferHelper();
	return res;
}

int8_t data_send_method(FIL *file, uint32_t start, uint32_t end) {
	if (g_pSysState->simState[g_pDevCfg->cfgSIM_slot].failsGPRS > 0 ||
			state_isGSM() || g_pDevCfg->cfgUploadMode == MODE_GSM) {
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

// If the last file was corrupted and forced a reboot we remove the extension
void cancel_batch(char *path, char *name) {
	char line[MAX_PATH];
	config_setLastCommand(COMMAND_CANCEL_BATCH);

	sprintf(path, "%s/%s", FOLDER_TEXT, name);

	strcpy(line,path);
	line[strlen(line)-3]=0;

	f_rename(path, line);
	http_deactivate();
	g_pSysState->safeboot.disable.data_transmit = 0;
	return;
}

void process_batch() {
	int8_t canSend = 0, transactionState = 0;
	uint32_t seekFrom = g_pSysState->lastSeek, seekTo = g_pSysState->lastSeek;

	FIL *filr = fat_getFile();
	FILINFO *fili = fat_getInfo();
	DIR *dir = fat_getDirectory();

	char path[32];
	char *line=NULL;

	config_setLastCommand(COMMAND_PROCESS_BATCH);
	FRESULT fr;

	line = getSMSBufferHelper();

	if (!state_isSimOperational()) {
 		return;
	}

	if (g_pSysState->simState[g_pDevCfg->cfgSIM_slot].failsGPRS == 0 && state_isGPRS()) {
		if (http_enable() != UART_SUCCESS) {
			g_pSysState->simState[g_pDevCfg->cfgSIM_slot].failsGPRS++;
		}
	}

	// Cycle through all files using f_findfirst, f_findnext.
	fr = f_findfirst(dir, fili, FOLDER_TEXT, "*." EXTENSION_TEXT);
	if (fr != FR_OK) {
		return;
	}

	// If the last file was corrupted and forced a reboot we remove the extension
	if (g_pSysState->safeboot.disable.data_transmit) {
		cancel_batch(path, fili->fname);
		return;
	}

	log_disable();
	g_pSysState->safeboot.disable.data_transmit = 1;

	while (fr == FR_OK) {
		sprintf(path, "%s/%s", FOLDER_TEXT, fili->fname);
		fr = fat_open(&filr, path, FA_READ | FA_OPEN_ALWAYS);
		if (fr != FR_OK) {
			http_deactivate();
			break;
		}

		lcd_printl(LINEC, "Transmitting...");
		lcd_printl(LINE2, fili->fname);

		if (g_pSysState->lastSeek > 0) {
			f_lseek(filr, g_pSysState->lastSeek);
		}

		// We reuse the temporal for the SMS to parse the line
		// otherwise we might run out of stack inside the sending function
		while (f_gets(line, MAX_SMS_SIZE, filr) != 0) {
			if (filr->fptr == 0 || strstr(line, "$TS") != NULL) {
				if (canSend) {
					canSend = 0;
					transactionState = data_send_method(filr, seekFrom, seekTo);
					if (transactionState != TRANS_SUCCESS) {
						break;
					}
					seekFrom = seekTo = filr->fptr;
					// Found next time stamp - Move to next batch now
				}
			} else {
				canSend = 1;
				seekTo = filr->fptr;
			}

			config_incLastCmd();
		}

		if(canSend) {
			transactionState = data_send_method(filr, seekFrom, seekTo);
			seekFrom = canSend = 0;
		}

		if (seekTo < filr->fsize && transactionState == TRANS_SUCCESS) {
			g_pSysState->lastSeek = filr->fptr;
		} else if (transactionState == TRANS_SUCCESS) {
			g_pSysState->lastSeek = 0;
		}

		if (fat_close() == FR_OK && transactionState == TRANS_SUCCESS) {
			if (g_pSysState->lastSeek == 0) {
				fr = f_unlink(path); // Delete the file
			}
		} else {
			break; // Tranmission failed
		}

		fr = f_findnext(dir, fili);
		if (strlen(fili->fname) == 0) {
			break;
		}
	}

	if (state_isGPRS()) {
		http_deactivate();
	}

	lcd_printl(LINEC, "Transmission");
	if (transactionState == TRANS_SUCCESS) {
		// Make sure this sim is the one that's first used next time
		g_pDevCfg->cfgSelectedSIM_slot = g_pDevCfg->cfgSIM_slot;
		lcd_printl(LINE2, "Completed");
	} else {
		lcd_printl(LINE2, "Failed");
	}
	g_pSysState->safeboot.disable.data_transmit = 0;

	log_enable();

	// End of transmit, lets save that we were successful
	config_setLastCommand(COMMAND_PROCESS_BATCH+99);
}
