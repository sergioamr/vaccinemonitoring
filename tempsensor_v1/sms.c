/*
 * sms.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#if 1
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

int8_t sms_process_msg(char* pSMSmsg) {
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
					strncpy(g_pInfoA->cfgSMSCenter[g_pInfoA->cfgSIMSlot], pcTmp,
							strlen(pcTmp));

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
								if (g_pInfoA->cfgSIMSlot == 0) {
									g_pInfoA->cfgSIMSlot = 1;
									modem_init();
								}
							} else {
								iStatus &= ~ENABLE_SECOND_SLOT;
								if (g_pInfoA->cfgSIMSlot == 1) {
									g_pInfoA->cfgSIMSlot = 0;
									modem_init();
								}
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

	iMsgRxPollElapsed = iMinuteTick;
	//check if messages are available
	uart_tx("AT+CPMS?\r\n");
	delay(1000);
	memset(ATresponse, 0, sizeof(ATresponse));
	uart_rx(ATCMD_CPMS, ATresponse);
	if (ATresponse[0] != 0) {
		iIdx = strtol(ATresponse, 0, 10);
		if (iIdx) {
			iIdx = 1;
			lcd_print_lne(LINE2, "Msg Processing..");
			while (iIdx <= SMS_READ_MAX_MSG_IDX) {
				memset(ATresponse, 0, sizeof(ATresponse));
				iModemSuccess = recvmsg(iIdx, ATresponse);
				if (ATresponse[0] != 0 && iModemSuccess == 0)//check for $
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
						strcat(SampleData, itoa_pad(iBatteryLevel));
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

						sendmsg_number(&ATresponse[6], SampleData);
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
			iModemSuccess = 0;
			delallmsg();
			lcd_show(iDisplayId);
		}
	}

}
void sms_send_heart_beat() {

	char* pcTmp = NULL;
	int slot = g_pInfoA->cfgSIMSlot;
	int i = 0;

	lcd_print("SMS SYNC");
	//send heart beat
	memset(SampleData, 0, sizeof(SampleData));
	strcat(SampleData, SMS_HB_MSG_TYPE);
	strcat(SampleData, g_pInfoA->cfgIMEI);
	strcat(SampleData, ",");
	if (slot) {
		strcat(SampleData, "1,");
	} else {
		strcat(SampleData, "0,");
	}
	strcat(SampleData, &g_pInfoA->cfgSMSCenter[slot][0]);
	strcat(SampleData, ",");
	strcat(SampleData, itoa_nopadding(g_pInfoA->iCfgMCC[slot]));
	strcat(SampleData, ",");
	strcat(SampleData, itoa_nopadding(g_pInfoA->iCfgMNC[slot]));
	strcat(SampleData, ",");
	for (i = 0; i < MAX_NUM_SENSORS; i++) {
		if(Temperature[i][0] == '-') {
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

#ifdef _DEBUG
	strcat(SampleData, ",(db)" __TIME__);
#endif
	sendmsg(SampleData);
}

extern char* itoa_nopadding(int num);

//#pragma SET_DATA_SECTION(".config_vars_infoD")
//char g_TmpSMScmdBuffer[SMS_CMD_LEN];
//#pragma SET_DATA_SECTION()

uint8_t sendmsg_number(char *szPhoneNumber, char* pData) {

	uint16_t msgNumber=0;  // Validation number from the network returned by the CMGS command
	int res=UART_ERROR;

	if (iStatus & TEST_FLAG)
		return UART_SUCCESS;

	if (g_iLCDVerbose == VERBOSE_BOOTING) {
		lcd_clear();
		lcd_print_lne(LINE1, "Sync SMS To ");
		lcd_print_lne(LINE2, szPhoneNumber);
	}

	lcd_disable_verbose();
	strcat(pData, ctrlZ);

	sprintf(g_szTemp, "AT+CMGS=\"%s\",129\r\n", szPhoneNumber);
	uart_setSMSPromptMode();
	if (uart_tx_waitForPrompt(g_szTemp, TIMEOUT_CMGS_PROMPT)) {
		uart_tx_timeout(pData, TIMEOUT_CMGS, 1);

		// TODO Check if ok or RXBuffer contains Error
		res = uart_rx(ATCMD_CMGS, ATresponse);
		msgNumber = atoi(ATresponse);
	}

	if (res == UART_SUCCESS) {
		lcd_clear();
		lcd_print_lne(LINE1, "SMS Confirmation");
		lcd_print_ext(LINE2, "MSG %d ", msgNumber);
		_NOP();
	} else if (res == UART_ERROR) {
		lcd_print("MODEM ERROR");
	}

	_NOP();
	return res;
}

uint8_t sendmsg(char* pData) {
	return sendmsg_number(SMS_NEXLEAF_GATEWAY, pData);
}

int recvmsg(int8_t iMsgIdx, char* pData) {
	int ret = -1;

	//reset the RX counters to reuse memory from POST DATA
	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;
	RXBuffer[RX_LEN] = 0; //null termination ... Outside the array??? wtf

	//uart_tx("AT+CMGL=\"REC UNREAD\"\r\n");
	//uart_tx("AT+CMGL=\"REC READ\"\r\n");
	//uart_tx("AT+CMGL=\"ALL\"\r\n");

	strcat(pData, "AT+CMGR="); //resuse to format CMGR (sms read)
	strcat(pData, itoa_nopadding(iMsgIdx));
	strcat(pData, "\r\n");
	uart_tx(pData);
	delay(1000);	//opt
	ret = uart_rx(ATCMD_CMGR, pData);	//copy RX to pData

	RXTailIdx = RXHeadIdx = 0;
	iRxLen = RX_LEN;

	return ret;
}

void delallmsg() {
	uart_tx("AT+CMGD=1,2\r\n");	//delete all the sms msgs read or sent successfully
}

void delmsg(int8_t iMsgIdx, char* pData) {

	strcat(pData, "AT+CMGD=");
	strcat(pData, itoa_nopadding(iMsgIdx));
	strcat(pData, ",0\r\n");
	uart_tx(pData);
	delay(2000);	//opt
}
#endif
