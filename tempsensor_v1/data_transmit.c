/*
 * data_transmit.c
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "buzzer.h"

#define TS_SIZE					21
#define TS_FIELD_OFFSET			1	//1 - $, 3 - $TS
#define TRANS_FAILED		 	-1
#define TRANS_SUCCESS			0

char *getSensorTemp(int sensorID) {
	static char sensorData[4];
	return sensorData;
}

uint8_t select_transmission_method() {
	if (modem_check_network() != UART_SUCCESS) {
		modem_swap_SIM();
		if (modem_check_network() != UART_SUCCESS) {
			// neither GSM or GPRS will work
			lcd_printl(LINEC, "Can't Complete");
			lcd_printl(LINE2, "No Signal...");
			return 2;
		}
	}

	if (http_enable() != UART_SUCCESS) {
		modem_swap_SIM();
		if (modem_check_network() != UART_SUCCESS &&
				http_enable() != UART_SUCCESS) {
			http_deactivate();
			return 1; // change to 1 for sms
		}
	}

	return 0;
}



uint8_t data_send_temperatures_sms() {
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
	return sms_send_message(data);
}

int8_t data_upload_sms(FIL *file, uint32_t start, uint32_t end) {
	char line[80];
	int lineSize = sizeof(line) / sizeof(char);
	char* dateString = NULL;
	struct tm firstDate;
	char smsMsg[MAX_SMS_SIZE];

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(smsMsg, "%d,%s,%s,%d,", 11, dateString,
				itoa_nopadding(g_pDevCfg->stIntervalParam.loggingInterval), 5);
	} else {
		return TRANS_FAILED;
	}

	uint8_t length = strlen(smsMsg);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, lineSize, file) != 0) {
			length += strlen(line);
			if (length > MAX_SMS_SIZE) {
				// Send what we can!
				break; // TODO break up dates and send seperate SMS texts.
			}

			if (file->fptr != end) {
				replace_character(line, '\n', ',');
			}

			encode_string(line, smsMsg, ",");
		} else {
			return TRANS_FAILED;
		}
	}

	if (sms_send_message(smsMsg) != UART_SUCCESS) {
		modem_swap_SIM();
		if (sms_send_message(smsMsg) != UART_SUCCESS) {
			return TRANS_FAILED;
		}
	}

	return TRANS_SUCCESS;
}

// 11,20150303:082208,interval,sensorid,DATADATADATAT,sensorid,DATADATADATA,
// sensorid,dATADATADA,sensorID,DATADATADATADATAT, sensorID,DATADATADATADATAT,batt level,battplugged.
// FORMAT = IMEI=...&ph=...&v=...&sid=.|.|.&sdt=...&i=.&t=.|.|.&b=...&p=...
int8_t http_send_batch(FIL *file, uint32_t start, uint32_t end) {
	int uart_state;
	char line[160];
	int retry = 0;
	int lineSize = sizeof(line)/sizeof(char);

	char* dateString = NULL;
	struct tm firstDate;

	SIM_CARD_CONFIG *sim = config_getSIM();

	f_lseek(file, start);

	// Must get first line before transmitting to calculate the length properly
	if (f_gets(line, lineSize, file) != 0) {
		parse_time_from_line(&firstDate, line);
		dateString = get_date_string(&firstDate, "", "", "", 0);
		sprintf(line, "IMEI=%s&ph=%s&v=%s&sdt=%s&i=%d&t=",
				g_pDevCfg->cfgIMEI, sim->cfgPhoneNum, "0.1pa", dateString,
				g_pDevCfg->stIntervalParam.loggingInterval);
	} else {
		return TRANS_FAILED;
	}

	uint32_t length = strlen(line) + (end - file->fptr);
	http_open_connection(length);

	// Send the date line
	uart_tx_nowait(line);
	lcd_progress_wait(1000);

	// check that the transmitted data equals the size to send
	while (file->fptr < end) {
		if (f_gets(line, lineSize, file) != 0) {
			if (file->fptr != end) {
				replace_character(line, '\n', '|');
				uart_tx_nowait(line);
				lcd_print_progress();
			} else {
				// Last line! Wait for OK
				uart_setHTTPDataMode();
				uart_tx_timeout(line, TIMEOUT_HTTPSND, 1); // We don't have more than one attempt to send data
				uart_setOKMode();
				uart_state = uart_getTransactionState();
				if (uart_state == UART_ERROR) {
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

void process_batch() {
	uint8_t canSend = 0, transmissionMethod = 0;
	uint32_t seekFrom = g_pSysCfg->lastSeek, seekTo = g_pSysCfg->lastSeek;
	char line[80];
	char path[32];
	char do_not_process_batch = false;
	int lineSize = sizeof(line) / sizeof(char);

	FILINFO fili;
	DIR dir;
	FIL filr;
	FRESULT fr;

	log_disable();

	transmissionMethod = select_transmission_method();
	if (transmissionMethod > 1) {
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

		lcd_printl(LINEC, "Transmitting...");
		lcd_printl(LINE2, fili.fname);

		if (g_pSysCfg->lastSeek > 0) {
			f_lseek(&filr, g_pSysCfg->lastSeek);
		}

		while (f_gets(line, lineSize, &filr) != 0) {
			if (filr.fptr == 0 || strstr(line, "$TS") != NULL) {
				if (canSend) {
					canSend = 0;
					if (transmissionMethod == 1) {
						if (data_upload_sms(&filr, seekFrom, seekTo) == TRANS_FAILED) {
							break;
						}
					} else {
						if (http_send_batch(&filr, seekFrom, seekTo) == TRANS_FAILED) {
							break;
						}
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
			if (transmissionMethod == 1) {
				if (data_upload_sms(&filr, seekFrom, seekTo) == TRANS_FAILED) {
					break;
				}
			} else {
				if (http_send_batch(&filr, seekFrom, seekTo) == TRANS_FAILED) {
					break;
				}
			}
			seekFrom = canSend = 0;
		}

		if (seekTo >= filr.fsize) {
			g_pSysCfg->lastSeek = 0;
		} else {
			g_pSysCfg->lastSeek = filr.fptr;
		}

		if (f_close(&filr) == FR_OK) {
			if (g_pSysCfg->lastSeek == 0) {
				fr = f_unlink(path); // Delete the file
			}
		} else {
			break; // This would only happen on corruption - In which case need to redo fat_init?
		}

		fr = f_findnext(&dir, &fili);
		if (strlen(fili.fname) == 0) {
			break;
		}
	}

	if (transmissionMethod == 1)
	http_deactivate();
	lcd_printl(LINEC, "Transmission");
	lcd_printl(LINE2, "Completed");
	g_pSysState->safeboot.disable.data_transmit = 0;
	g_iStatus |= LOG_TIME_STAMP; // Uploads may take a long time and might require offset to be reset
	log_enable();
}
