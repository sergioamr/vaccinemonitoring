/*
 * http.c
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

//uart_tx("AT+CGDCONT=1,\"IP\",\"www\",\"0.0.0.0\",0,0\r\n");
#include "thermalcanyon.h"
#include "stringutils.h"

#define HTTP_RESPONSE_RETRY	10

int8_t http_setup() {
	int uart_state = UART_FAILED;
	int attempts = HTTP_COMMAND_ATTEMPTS;
	SIM_CARD_CONFIG *sim = config_getSIM();

	if (sim->cfgAPN[0] == '\0')
		return UART_FAILED;

	lcd_printf(LINEC, "Testing HTTP %d", config_getSelectedSIM() + 1);

	uart_txf("AT+CGDCONT=1,\"IP\",\"%s\",\"0.0.0.0\",0,0\r\n", sim->cfgAPN); //APN
	if (uart_getTransactionState() != UART_SUCCESS)
		return UART_FAILED;

	lcd_progress_wait(2000);

	// Context Activation - #SGACT
	// Execution command is used to activate or deactivate either the GSM context
	// or the specified PDP context.
	//  1..5 - numeric parameter which specifies a particular PDP context definition
	//  1 - activate the context

	do {
		uart_tx("AT#SGACT=1,1\r\n");
		// CME ERROR: 555 Activation failed
		// CME ERROR: 133 Requested service option not subscribed
		uart_state = uart_getTransactionState();
		if (uart_state != UART_SUCCESS) {
			delay(1000);
		} else {
			config_reset_error(sim);
		}

	} while (uart_state != UART_SUCCESS && --attempts > 0);

	// We were not successful trying to activate the HTTP transaction
	if (uart_state != UART_SUCCESS)
		return UART_FAILED;

	// LONG TIMEOUT
	uart_txf("AT#HTTPCFG=1,\"%s\",80\r\n", g_pDeviceCfg->cfgGatewayIP);
	if (uart_getTransactionState() != UART_SUCCESS) {
		lcd_printl(LINE2, "FAILED");
		return UART_FAILED;
	}

	lcd_printl(LINE2, "SUCCESS");
	return UART_SUCCESS;
}

void http_deactivate() {
	// LONG TIMEOUT
	uart_tx("AT#SGACT=1,0\r\n");	//deactivate GPRS context
}

// http://54.241.2.213/coldtrace/uploads/multi/v3/358072043112124/1/
// $ST2,7,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,20,20,20,30,600,1,600,15,$ST1,919243100142,both,airtelgprs.com,10,1,0,0,$EN

const char HTTP_INCOMING_DATA[] = { 0x0D, 0x0A, '<', '<', '<', 0 };

int process_configuration() {
	char *token;
	char* delimiter = ",";
	SIM_CARD_CONFIG *sim = config_getSIM();

	//change for actual data
	PARSE_FINDSTR_RET(token, HTTP_INCOMING_DATA, UART_FAILED);
	PARSE_FINDSTR_RET(token, "$ST2", UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->cfgSyncId, delimiter, UART_FAILED);
	int i = 0;
	while (i < MAX_NUM_SENSORS) {
		PARSE_NEXTVALUE(token, &g_pDeviceCfg->stTempAlertParams[i].maxTimeCold, delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDeviceCfg->stTempAlertParams[i].threshCold, delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDeviceCfg->stTempAlertParams[i].maxTimeHot, delimiter, UART_FAILED);
		PARSE_NEXTVALUE(token, &g_pDeviceCfg->stTempAlertParams[i].threshHot, delimiter, UART_FAILED);
	}

	// HOWEVER MANY FOR THE BATTERY INFO
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stBattPowerAlertParam.minutesPower, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stBattPowerAlertParam.enablePowerAlert, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stBattPowerAlertParam.minutesBattThresh, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stBattPowerAlertParam.battThreshold, delimiter, UART_FAILED);

	PARSE_SKIP(token, ",", UART_FAILED); // $ST1
	PARSE_NEXTSTRING(token, g_pDeviceCfg->cfgGatewaySMS, strlen(token), delimiter, UART_FAILED); // GATEWAY NUM
	PARSE_NEXTVALUE(token, &sim->cfgUploadMode, delimiter, UART_FAILED); // NETWORK TYPE E.G. GPRS
	PARSE_NEXTSTRING(token, sim->cfgAPN, strlen(token), delimiter, UART_FAILED); //APN

	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stIntervalParam.uploadInterval, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->stIntervalParam.loggingInterval, delimiter, UART_FAILED);
	PARSE_NEXTVALUE(token, ",", delimiter, UART_FAILED); // Reset alert
	PARSE_NEXTVALUE(token, &g_pDeviceCfg->cfgSIM_slot, delimiter, UART_FAILED); ////

	PARSE_SKIP(token, delimiter, UART_FAILED); // $EN

	return UART_SUCCESS;
}

const char HTTP_ERROR[] = { 0x0D, 0x0A, 'E', 'R', 'R', 'O', 'R', 0x0D, 0x0A,
		0x0D, 0x0A, 0 };
const char HTTP_OK[] = { 0x0D, 0x0A, 'O', 'K', 0x0D, 0x0A, 0 };
const char HTTP_RING[] = "#HTTPRING: ";

// <prof_id>,<http_status_code>,<content_type>,<data_size>
int http_check_error(int *retry) {

	char *token = NULL;
	int prof_id = 0;
	int http_status_code = 0;
	int data_size = 0;

	// Parse HTTPRING
	PARSE_FINDSTR_RET(token, HTTP_RING, UART_FAILED);

	PARSE_FIRSTVALUE(token, &prof_id, ",\n", UART_FAILED);
	PARSE_NEXTVALUE(token, &http_status_code, ",\n", UART_FAILED);
	PARSE_SKIP(token, ",\n", UART_FAILED); 	// Skip content_type string.
	PARSE_NEXTVALUE(token, &data_size, ",\n", UART_FAILED);

	log_appendf("HTTP ERROR %i [%d] - Data size %d ", prof_id, http_status_code, data_size);

	// Check for recoverable errors
	// Server didnt responded
	if (http_status_code==200 && data_size==0) {
		*retry=1;
		lcd_printl(LINEC, "HTTP Server");
		lcd_printl(LINEH, "Empty response");
	}

	// TODO Find non recoverable errors
	return UART_SUCCESS;
}

int http_get_configuration() {
	char szTemp[128];
	int uart_state = UART_FAILED;
	int retry = 1;
	int attempts = HTTP_COMMAND_ATTEMPTS;

	// #HTTPQRY � send HTTP GET, HEAD or DELETE request
	// Execution command performs a GET, HEAD or DELETE request to HTTP server.
	// Parameters:
	// <prof_id> - Numeric parameter indicating the profile identifier. Range: 0-2
	// <command>: Numeric parameter indicating the command requested to HTTP server:
	// 0 � GET 1 � HEAD 2 � DELETE

	sprintf(szTemp, "AT#HTTPQRY=1,0,\"/coldtrace/uploads/multi/v3/%s/1/\"\r\n",
			g_pDeviceCfg->cfgIMEI);
	uart_tx_timeout(szTemp, 5000, 1);
	if (uart_getTransactionState() != UART_SUCCESS)
		return UART_FAILED;

	// CME ERROR: 558 cannot resolve DN ?

	// Get the configration from the server
	// #HTTPRCV � receive HTTP server data

	uart_setCheckMsg(HTTP_OK, HTTP_ERROR);

	while(retry==1 && --attempts>0) {
		if (uart_tx_timeout("AT#HTTPRCV=1\r\n", 180000, 5) == UART_SUCCESS) {
			uart_state = uart_getTransactionState();
			if (uart_state == UART_SUCCESS) {
				retry = 0; 	// Found a configuration, lets parse it.
				process_configuration();
			}

			if (uart_state == UART_ERROR)
				http_check_error(&retry);  // Tries again
		}
	};

	return uart_state; // TODO return was missing, is it necessary ?
}

int http_post_sms_status(void) {
	int l_file_pointer_enabled_sms_status = 0;
	int isHTTPResponseAvailable = 0;
	int i = 0, j = 0;

	if (g_iStatus & TEST_FLAG)
		return l_file_pointer_enabled_sms_status;
	iHTTPRespDelayCnt = 0;
	while ((!isHTTPResponseAvailable)
			&& iHTTPRespDelayCnt <= HTTP_RESPONSE_RETRY) {
		delay(1000);
		iHTTPRespDelayCnt++;
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C')
					&& (RXBuffer[j + 1] == 'M') && (RXBuffer[j + 2] == 'G')
					&& (RXBuffer[j + 3] == 'S') && (RXBuffer[j + 4] == ':')) {
				isHTTPResponseAvailable = 1;
				l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
			}
		}
	}
	if (isHTTPResponseAvailable == 0) {
		l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
	}
	return l_file_pointer_enabled_sms_status;
}

int http_post_gprs_connection_status(char status) {
	int l_file_pointer_enabled_sms_status = 0;
	int i = 0, j = 0;

	if (g_iStatus & TEST_FLAG)
		return l_file_pointer_enabled_sms_status;

	iHTTPRespDelayCnt = 0;

	if (status == GSM) {
		uart_tx("AT+CREG?\r\n");
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}
			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C')
					&& (RXBuffer[j + 1] == 'R') && (RXBuffer[j + 2] == 'E')
					&& (RXBuffer[j + 3] == 'G') && (RXBuffer[j + 4] == ':')) {
				if (RXBuffer[j + 8] == '1') {
					l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
					break;
				} else {
					l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
					break;
				}
			}
		}
	}

	//////
	if (status == GPRS) {
		// Network Registration Report
		uart_tx("AT+CGREG?\r\n");
		for (i = 0; i < 102; i++) {
			j = i + 1;
			if (j > 101) {
				j = j - 101;
			}

			if ((RXBuffer[i] == '+') && (RXBuffer[j] == 'C')
					&& (RXBuffer[j + 1] == 'G') && (RXBuffer[j + 2] == 'R')
					&& (RXBuffer[j + 3] == 'E') && (RXBuffer[j + 4] == 'G')
					&& (RXBuffer[j + 5] == ':')) {
				if ((RXBuffer[j + 7] == '0') && (RXBuffer[j + 8] == ',')
						&& (RXBuffer[j + 9] == '1')) {
					l_file_pointer_enabled_sms_status = 1; // set for sms packet.....///
					break;
				} else {
					delay(1000);
					uart_tx("AT+CGATT=1\r\n");
					l_file_pointer_enabled_sms_status = 0; // reset for sms packet.....///
					break;
				}
			}
		}
	}

	return l_file_pointer_enabled_sms_status;
}

int http_post(char* postdata) {
	int isHTTPResponseAvailable = 0;
	int iRetVal = -1;

	if (g_iStatus & TEST_FLAG)
		return iRetVal;
#ifndef SAMPLE_POST
	memset(ATresponse, 0, sizeof(ATresponse));
#ifdef NOTFROMFILE
	strcpy(ATresponse,"AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",");
#else
	strcpy(ATresponse, "AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v3/\",");
#endif
	strcat(ATresponse, itoa_pad(strlen(postdata)));
	strcat(ATresponse, ",0\r\n");
	//uart_tx("AT#HTTPCFG=1,\"54.241.2.213\",80\r\n");
	//delay(5000);

	isHTTPResponseAvailable = 0;
	//uart_tx("AT#HTTPSND=1,0,\"/coldtrace/uploads/multi/v2/\",383,0\r\n");

	// Wait for prompt

	uart_setHTTPPromptMode();
	if (uart_tx_waitForPrompt(ATresponse, TIMEOUT_HTTPSND_PROMPT)) {
		uart_tx_timeout(postdata, TIMEOUT_HTTPSND, 1);

		// TODO Check if ok or RXBuffer contains Error
		uart_rx(ATCMD_HTTPSND, ATresponse);
		if (ATresponse[0] == '>') {
			isHTTPResponseAvailable = 1;
		} else {
			_NOP();
			// FAILED GETTING PROMPT!
			return iRetVal;
		}
	}

	if (isHTTPResponseAvailable) {
		file_pointer_enabled_gprs_status = 1; // added for gprs status for file pointer movement///
		uart_tx(postdata);
#if 0
		//testing split http post
		memset(filler,0,sizeof(filler));
		memcpy(filler, postdata, 200);
		uart_tx(filler);
		delay(10000);
		memset(filler,0,sizeof(filler));
		memcpy(filler, &postdata[200], iLen - 200);
		uart_tx(filler);
#endif
		//delay(10000);
		iRetVal = 0;
		iPostSuccess++;
	} else {
		file_pointer_enabled_gprs_status = 0; // added for gprs status for file pointer movement..////
		iPostFail++;
	}
#else
	uart_tx("AT#HTTPCFG=1,\"67.205.14.22\",80,0,,,0,120,1\r\n");
	delay(5000);
	uart_tx("AT#HTTPSND=1,0,\"/post.php?dir=galcore&dump\",26,1\r\n");
	isHTTPResponseAvailable = 0;
	iIdx = 0;
	while ((!isHTTPResponseAvailable) && iIdx <= HTTP_RESPONSE_RETRY)
	{
		delay(1000);
		iIdx++;
		uart_rx(ATCMD_HTTPSND,ATresponse);
		if(ATresponse[0] == '>')
		{
			isHTTPResponseAvailable = 1;
		}
	}

	if(isHTTPResponseAvailable)
	{
		iRetVal = 1;
		uart_tx("imessy=martina&mary=johyyy");
		delay(10000);
	}
#endif
	return iRetVal;
}

int16_t formatfield(char* pcSrc, char* fieldstr, int lastoffset,
		char* seperator, int8_t iFlagVal, char* pcExtSrc, int8_t iFieldSize) {
	char* pcTmp = NULL;
	char* pcExt = NULL;
	int32_t iSampleCnt = 0;
	int16_t ret = 0;
	int8_t iFlag = 0;

	//pcTmp = strstr(pcSrc,fieldstr);
	pcTmp = strchr(pcSrc, fieldstr[0]);
	iFlag = iFlagVal;
	while ((pcTmp && !lastoffset)
			|| (pcTmp && (char *) lastoffset && (pcTmp < (char *) lastoffset))) {
		iSampleCnt = lastoffset - (int) pcTmp;
		if ((iSampleCnt > 0) && (iSampleCnt < iFieldSize)) {
			//the field is splitted across
			//reuse the SampleData tail part to store the complete timestamp
			pcExt = &SampleData[SAMPLE_LEN - 1] - iFieldSize - 1;//to accommodate field pair and null termination
			//memset(g_TmpSMScmdBuffer,0,SMS_CMD_LEN);
			//pcExt = &g_TmpSMScmdBuffer[0];
			memset(pcExt, 0, iFieldSize + 1);
			memcpy(pcExt, pcTmp, iSampleCnt);
			memcpy(&pcExt[iSampleCnt], pcExtSrc, iFieldSize - iSampleCnt);
			pcTmp = pcExt;	//point to the copied field
		}

		iSampleCnt = 0;
		while (pcTmp[FORMAT_FIELD_OFF + iSampleCnt] != ',')	//TODO recheck whenever the log formatting changes
		{
			iSampleCnt++;
		}
		if (iFlag) {
			//non first instance
			strcat(SampleData, ",");
			strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
		} else {
			//first instance
			if (seperator) {
				strcat(SampleData, seperator);
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			} else {
				strncat(SampleData, &pcTmp[FORMAT_FIELD_OFF], iSampleCnt);
			}
		}
		//pcTmp = strstr(&pcTmp[3],fieldstr);
		pcTmp = strchr(&pcTmp[FORMAT_FIELD_OFF], fieldstr[0]);
		iFlag++;
		ret = 1;
	}

	return ret;
}

