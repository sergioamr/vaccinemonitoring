/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#define SMS_C_

#include "stdint.h"
#include "stdio.h"
#include "stringutils.h"
#include "sms.h"
#include "uart.h"
#include "timer.h"
#include "config.h"
#include "common.h"
#include "lcd.h"
#include "string.h"
#include "globals.h"
#include "modem.h"
#include "stdlib.h"
#include "battery.h"
#include "pmm.h"
#include "fatdata.h"

int8_t sms_process_msg(char* pSMSmsg) {
	char* pcTmp = NULL;
	int8_t iCnt = 0;
	int8_t iCurrSeq = 0;
	SIM_CARD_CONFIG *sim = config_getSIM();

	pcTmp = strtok(pSMSmsg, ",");

	while ((pcTmp) && (iCurrSeq != -1)) {
		if (pcTmp) {
			//get msg type
			switch (pcTmp[3]) {
			case '1':
				pcTmp = strtok(NULL, ",");
				if (pcTmp) {
					strncpy(sim->cfgSMSCenter, pcTmp, strlen(pcTmp));

					pcTmp = strtok(NULL, ",");
					if (pcTmp) {
						//get upload mode
						sim->cfgUploadMode = strtol(pcTmp, 0, 10);
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							//get APN
							memcpy(sim->cfgAPN, pcTmp, strlen(pcTmp));
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
							//set buzzer OFF
							g_iStatus &= ~BUZZER_ON;
							//reset alarm state and counters
							for (iCnt = 0; iCnt < MAX_NUM_SENSORS; iCnt++) {
								//reset the alarm
								TEMP_ALARM_CLR(iCnt);
								//initialize confirmation counter
								g_iAlarmCnfCnt[iCnt] = 0;
							}
						} else {
							g_iStatus &= ~RESET_ALERT;
						}
						pcTmp = strtok(NULL, ",");
						if (pcTmp) {
							if (strtol(pcTmp, 0, 10)) {
								g_iStatus |= ENABLE_SECOND_SLOT;
								modem_swap_SIM();
							} else {
								g_iStatus &= ~ENABLE_SECOND_SLOT;
								modem_swap_SIM();
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
						g_pInfoA->stBattPowerAlertParam.minutespower = strtol(
								pcTmp, 0, 10);
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

// TODO to be tested
void sms_process_messages(uint32_t iMinuteTick, uint8_t iDisplayId) {
	uint32_t iIdx, iOffset;
	SIM_CARD_CONFIG *sim = config_getSIM();

	iMsgRxPollElapsed = iMinuteTick;
	//check if messages are available
	uart_tx("AT+CPMS?\r\n");
	uart_rx(ATCMD_CPMS_CURRENT, ATresponse);
	if (ATresponse[0] != '0') {
		iIdx = strtol(ATresponse, 0, 10);
		if (iIdx) {
			iIdx = 1;
			lcd_printl(LINE2, "Msg Processing..");
			if (sim->iMaxMessages != 0xFF && sim->iMaxMessages != 0x00) {
				while (iIdx < sim->iMaxMessages) {
					iModemSuccess = sms_recv_message(iIdx, ATresponse);
					if (ATresponse[0] != 0 && iModemSuccess == ((int8_t) 0)) {
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
							strcat(SampleData, itoa_pad(g_iBatteryLevel));
							strcat(SampleData, "%, ");
							if (P4IN & BIT4)	//power not plugged
							{
								strcat(SampleData, "POWER OUT");
							} else if (((P4IN & BIT6))
									&& (g_iBatteryLevel == 100)) {
								strcat(SampleData, "FULL CHARGE");
							} else {
								strcat(SampleData, "CHARGING");
							}
							iOffset = strlen(SampleData);

							sms_send_message_number(&ATresponse[6], SampleData);
							break;

						case '2':
							//reset the board by issuing a SW BOR
							PMM_trigBOR();
							while (1)
								;	//code should not come here
						default:
							break;
						}
						if (sms_process_msg(ATresponse)) {
							//send heartbeat on successful processing of SMS message
							sms_send_heart_beat();
						}
					}
					iIdx++;
				}
			} else {
				modem_set_max_messages();
			}
			iModemSuccess = 0;
			delallmsg();
			lcd_show(iDisplayId);
		}
	}
}

void sms_send_heart_beat() {

	char* pcTmp = NULL;
	SIM_CARD_CONFIG *sim = config_getSIM();

	int i = 0;

	lcd_print("SMS SYNC");
	//send heart beat
	memset(SampleData, 0, sizeof(SampleData));
	strcat(SampleData, SMS_HB_MSG_TYPE);
	strcat(SampleData, g_pInfoA->cfgIMEI);
	strcat(SampleData, ",");
	if (config_getSelectedSIM()) {
		strcat(SampleData, "1,");
	} else {
		strcat(SampleData, "0,");
	}
	strcat(SampleData, g_pInfoA->cfgGateway);
	strcat(SampleData, ",");
	strcat(SampleData, sim->cfgSMSCenter);
	strcat(SampleData, ",");
	for (i = 0; i < MAX_NUM_SENSORS; i++) {
		if (Temperature[i][0] == '-') {
			strcat(SampleData, "0,");
		} else {
			strcat(SampleData, "1,");
		}
	}

	pcTmp = itoa_pad(batt_getlevel());	//opt by directly using tmpstr
	strcat(SampleData, pcTmp);
	if (P4IN & BIT4) {
		strcat(SampleData, ",0");
	} else {
		strcat(SampleData, ",1");
	}

	// Attach Fimware release date
	strcat(SampleData, ",");
	strcat(SampleData, g_pSysCfg->firmwareVersion);
#ifdef _DEBUG
	strcat(SampleData, " dev");
#endif
	strcat(SampleData, "");

	sms_send_message(SampleData);
}

extern char* itoa_nopadding(int num);

//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()

uint8_t sms_send_message_number(char *szPhoneNumber, char* pData) {
	char szCmd[64];
	uint16_t msgNumber = 0; // Validation number from the network returned by the CMGS command
	int res = UART_ERROR;
	int verbose = g_iLCDVerbose;

	if (g_iStatus & TEST_FLAG)
		return UART_SUCCESS;

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_clear();
		lcd_printf(LINE1, "SYNC SMS %d ", g_pInfoA->cfgSIM_slot + 1);
		lcd_printl(LINE2, szPhoneNumber);
		lcd_disable_verbose();
	}

	strcat(pData, ctrlZ);

	sprintf(szCmd, "AT+CMGS=\"%s\",129\r\n", szPhoneNumber);
	uart_setSMSPromptMode();
	if (uart_tx_waitForPrompt(szCmd, TIMEOUT_CMGS_PROMPT)) {
		uart_tx_timeout(pData, TIMEOUT_CMGS, 1);

		// TODO Check if ok or RXBuffer contains Error
		res = uart_rx(ATCMD_CMGS, ATresponse);
		msgNumber = atoi(ATresponse);
	}

	if (verbose == VERBOSE_BOOTING)
		lcd_enable_verbose();

	if (res == UART_SUCCESS) {
		lcd_clear();
		lcd_printl(LINE1, "SMS Confirm    ");
		lcd_printf(LINE2, "MSG %d ", msgNumber);
		delay(HUMAN_DISPLAY_INFO_DELAY);
		_NOP();
	} else if (res == UART_ERROR) {
		lcd_printf(LINE2, "MODEM ERROR     ");
		log_appendf("ERROR: SIM %d FAILED [%s]", config_getSelectedSIM(),
				config_getSIM()->simLastError);
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	} else {
		lcd_print("MODEM TIMEOUT");
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	}

	_NOP();
	return res;
}

uint8_t sms_send_message(char* pData) {
	return sms_send_message_number(SMS_NEXLEAF_GATEWAY, pData);
}

int sms_recv_message(int8_t iMsgIdx, char* pData) {
	int ret = -1;

	//uart_tx("AT+CMGL=\"REC UNREAD\"\r\n");
	//uart_tx("AT+CMGL=\"REC READ\"\r\n");
	//uart_tx("AT+CMGL=\"ALL\"\r\n");

	uart_txf("AT+CMGR=%d\r\n", iMsgIdx);
	ret = uart_rx(ATCMD_CMGR, pData);	//copy RX to pData

	return ret;
}

void delallmsg() {
	uart_tx("AT+CMGD=1,2\r\n");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx, char* pData) {
	char szCmd[64];
	sprintf(szCmd, "AT+CMGD=%d,0\r\n", iMsgIdx);
	uart_tx(szCmd);
	delay(2000);	//opt
}

