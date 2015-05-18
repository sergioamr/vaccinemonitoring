/*
 * uart.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#include "config.h"
#include "common.h"
#include "uart.h"
#include "stdlib.h"
#include "string.h"
#include "lcd.h"

#define DEBUG_INFO

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile char RX[RX_LEN+1];
#pragma SET_DATA_SECTION()

volatile int RXTailIdx = 0;
volatile int RXHeadIdx = 0;
//volatile char TX[TX_LEN] ="";
volatile char* TX;
volatile int TXIdx = 0;
volatile int8_t iTxStop = 0;
volatile uint8_t iError = 0;
volatile uint8_t iTXInProgress = 0;
int iTxLen = 0;
int iRxLen = RX_LEN;
#define INDEX_2 2
#define INDEX_5 5
#define INDEX_7 7
#define INDEX_9 9
#define INDEX_12 12
#define MIN_OFFSET  0x30
#define MAX_OFFSET 0x39

#define IMEI_MAX_LEN		 15
//char Substr[TOKEN_LEN+1];

//local functions
static int searchtoken(char* pToken, char** ppTokenPos);
extern void delay(int time);

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG))
  {
    case USCI_NONE: break;
    case USCI_UART_UCRXIFG:
      if(UCA0STATW & UCRXERR)
      {
      	iError++;
      }
      RX[RXTailIdx] = UCA0RXBUF;
      if(RX[RXTailIdx] == XOFF)
      {
    	  iTxStop = 1;
      }
      else if(RX[RXTailIdx] == XON)
      {
    	  iTxStop = 0;
      }

      RXTailIdx = (RXTailIdx + 1);
      if(RXTailIdx >= iRxLen)
      {
    	  RXTailIdx = 0;
      }
      __no_operation();
      break;
    case USCI_UART_UCTXIFG:
        if((TXIdx < iTxLen) && (iTXInProgress))
        {
      	  UCA0TXBUF = TX[TXIdx];
      	  TXIdx = (TXIdx + 1);
        }
        else
        {
      	  //UCA0IE &= ~UCTXIE;
      	  iTXInProgress = 0;
        }

    break;
    case USCI_UART_UCSTTIFG: break;
    case USCI_UART_UCTXCPTIFG:
    break;
  }
}

void uart_resetbuffer() {
	RXHeadIdx = RXTailIdx = 0;//ZZZZ reset Rx index to faciliate processing in uart_rx
}

int uart_tx(volatile char* pTxData)
{
	int ret = -1;

#ifdef _DEBUG
	if (g_iDebug_state == 0) {
		lcd_clear();
		lcd_print_debug((char *) pTxData, LINE1);
	}
#endif

	if(pTxData)
	{
		while(iTXInProgress == 1);		//wait till the last msg is transmitted

		iTxLen = strlen((const char *) pTxData);
		//memset(TX,0,sizeof(TX));
		//memcpy(TX,pTxData,iTxLen);
		TX = pTxData;
		TXIdx = 1;
		ret = 0;
		iTXInProgress = 1;				//next message is ready
		UCA0IE |= UCTXIE;
		UCA0TXBUF = TX[0];
		//__bis_SR_register(GIE);
		//UCA0IE |= UCTXCPTIE;
	}

#ifdef _DEBUG
	memset((char *) RX, sizeof(RX),0);
#endif

	return ret;
}

int uart_rx(int atCMD, char* pResponse)
{
	int  ret = -1;
	char* pToken1 = NULL;
	char* pToken2 = NULL;
	int  bytestoread = 0;
	int  iStartIdx = 0;
	int  iEndIdx = 0;

	if (RXHeadIdx < RXTailIdx) {
		pToken1 = strstr((const char *) &RX[RXHeadIdx], "CMS ERROR:");
		if (pToken1 != NULL) {
			// ERROR FOUND;
			lcd_clear();
			lcd_print_debug((char *) &RX[RXHeadIdx + 7], LINE1);
			delay(5000);
			return ret;
		} else {
			#if defined(DEBUG_INFO) && defined(_DEBUG)
				lcd_print_debug((char *) RX, LINE2);
				delay(2000);
			#endif
		}
	}

	//input check
	if(pResponse)
	{
		switch(atCMD)
		{
			// Getting the message center to relay the SMS to the carrier
			case ATCMD_CSCA:
				pToken1 = strstr((const char *) RX,"CSCA:");
				if(pToken1 != NULL) {
					// TODO Parse the format "+CSCA: address,address_type"
				}
			break;

			case ATCMD_CCLK:
				pToken1 = strstr((const char *) RX,"CCLK:");
				if(pToken1 != NULL)
				{
					pToken2 = strstr(pToken1,"OK");

					if(pToken2 != NULL)
					{
						bytestoread = pToken2 - pToken1;
						if(bytestoread > CCLK_RESP_LEN)
						{
							bytestoread = CCLK_RESP_LEN;
						}
					}
					else
					{
						bytestoread = CCLK_RESP_LEN;
					}
					memcpy(pResponse, pToken1, bytestoread);
					ret = 0;
				}
				else{

				}

			break;

			case ATCMD_CSQ:
				pToken1 = strstr((const char *) RX,"CSQ:");
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
					pToken2 = strtok(&pToken1[5],",");

					if(pToken2 != NULL)
					{
						strncpy(pResponse,pToken2,strlen(pToken2));
					}
					else
					{
						pResponse[0] = 0;
					}
					ret = 0;
				}
			break;

			case ATCMD_CPMS:
				pToken1 = strstr((const char *) RX,"CPMS:");
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
					pToken2 = strtok(&pToken1[5],",");

					if(pToken2 != NULL)
					{
						pToken2 = strtok(NULL,",");
						strncpy(pResponse,pToken2,strlen(pToken2));
					}
					else
					{
						pResponse[0] = 0;
					}
					ret = 0;
				}
			break;

			case ATCMD_CGSN:
			 pToken1 = strstr((const char *) RX,"OK");
			 if(pToken1 != NULL){
			   if((RX[INDEX_2] >= MIN_OFFSET)&&(RX[INDEX_2] <= MAX_OFFSET)&&(RX[INDEX_5] >= MIN_OFFSET)&&(RX[INDEX_5] <= MAX_OFFSET)&&(RX[INDEX_7] >= MIN_OFFSET)&&(RX[INDEX_7] <= MAX_OFFSET) && (RX[INDEX_9] >= MIN_OFFSET)&& (RX[INDEX_9] <= MAX_OFFSET)&& (RX[INDEX_12] >= MIN_OFFSET)&&(RX[INDEX_12] <= MAX_OFFSET)){
				  strncpy(pResponse,(const char *) &RX[2],IMEI_MAX_LEN);
               }
			   else{
				  strcpy(pResponse,"INVALID IMEI");
			   }
				ret=0;
			}						//	break;
			case ATCMD_HTTPSND:
				pToken1 = strstr((const char *) &RX[RXHeadIdx],">>>");
				if(pToken1 != NULL)
				{
					//bytestoread = &RX[RXTailIdx] - pToken1;
					//if(bytestoread > HTTPSND_RSP_LEN)
					//{
						//bytestoread = HTTPSND_RSP_LEN;
					//}
					//memcpy(pResponse, pToken1, bytestoread);

					//need to esnure that RXTailIdx is not moving. i.e disable any asynchronous
					//notification
					if (((RXTailIdx > RXHeadIdx) && (pToken1 < &RX[RXTailIdx])) ||
						 (RXTailIdx < RXHeadIdx))
					{
						strcpy(pResponse,">>>");
						RXHeadIdx = RXTailIdx;
						ret = 0;
					}
				}
				else if(RXTailIdx < RXHeadIdx)
				{
					//rollover
					pToken1 = strstr((const char *) RX,">>>");
					if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
					{
						strcpy(pResponse,">>>");
						RXHeadIdx = RXTailIdx;
						ret = 0;
					}
					//check http post response is splitted
					else if(((RX[iRxLen-2] == '>') && (RX[iRxLen-1] == '>') && (RX[0] == '>')) ||
							((RX[iRxLen-1] == '>') && (RX[0] == '>') && (RX[1] == '>')))
					{
						strcpy(pResponse,">>>");
						RXHeadIdx = RXTailIdx;
						ret = 0;
					}
					else
					{
						pResponse[0] = 0;
					}

				}
				break;

			case ATCMD_CMGR:
				//check if this msg is unread
				iStartIdx = 0;
				pToken1 = strstr((const char *) RX,"UNREAD");
				//pToken1 = strstr(RX,"READ");
				//only STATUS and RESET are supported
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
					pToken1 = strstr((const char *) RX,"STATUS");
					if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
					{
						strncpy(pResponse,"1,",2);
						iStartIdx = 1;	//indicates that msg is valid
					}
					else
					{
						pToken1 = strstr((const char *) RX,"RESET");
						if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
						{
							strncpy(pResponse,"2,",2);
							iStartIdx = 1; //indicates that msg is valid
						}
					}
				}

				if(iStartIdx)
				{
					iEndIdx = 0;
					pToken1 = strtok((char *) RX,",");
					while(pToken1)
					{
#if 0
						iEndIdx++;
						if(iEndIdx == 9)	//10th field is sender's phone number
						{
							break;
						}
						else
						{
							pToken1 = strtok(NULL,",");
						}
#endif
						pToken2 = strstr(pToken1,"+91");
						if(pToken2)
						{
							break;
						}
						pToken1 = strtok(NULL,",");

					}
					//check if not null, this token contains the senders phone number
					if(pToken1)
					{
						strncpy(&pResponse[2],pToken1,PHONE_NUM_LEN);
					}
					else
					{
						pResponse[0] = 0; //ingore the message
					}

				}
				break;

			case ATCMD_CMGL:
				//check if this msg is unread
				pToken1 = strstr((const char *) RX,"UNREAD");
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
					if(searchtoken("$ST", &pToken1) == 0)
					{
						RXHeadIdx = pToken1 - &RX[0];
						iStartIdx = RXHeadIdx;

						//memset(Substr,0,sizeof(Substr));
						//strcpy(Substr,"$EN");
						if(searchtoken("$EN", &pToken2) == 0)
						{
							RXHeadIdx = pToken2 - &RX[0];
							iEndIdx = RXHeadIdx;

							if(iEndIdx > iStartIdx)
							{
								//no rollover
								bytestoread = iEndIdx - iStartIdx;
								if(bytestoread > CMGL_RSP_LEN)
								{
									bytestoread = CMGL_RSP_LEN;
								}
								memcpy(pResponse,(const char *) &RX[iStartIdx],bytestoread);
								ret = 0;
							}
							else
							{
								//rollover
								bytestoread = iRxLen - iStartIdx;
								if(bytestoread > CMGL_RSP_LEN)
								{
									bytestoread = CMGL_RSP_LEN;
								}
								memcpy(pResponse,(const char *) &RX[iStartIdx],bytestoread);
								memcpy(&pResponse[bytestoread],(const char *) RX,iEndIdx);
								ret = 0;
							}
						}
					}
				}
				else
				{
					//no start tag found
					pResponse[0] = 0;
				}
				break;

			case ATCMD_HTTPRCV:
				pToken1 = strstr((const char *) RX,"HTTPRING");
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
					if(searchtoken("$ST", &pToken1) == 0)
					{
						RXHeadIdx = pToken1 - &RX[0];
						iStartIdx = RXHeadIdx;

						//memset(Substr,0,sizeof(Substr));
						//strcpy(Substr,"$EN");
						if(searchtoken("$EN", &pToken2) == 0)
						{
							RXHeadIdx = pToken2 - &RX[0];
							iEndIdx = RXHeadIdx;

							if(iEndIdx > iStartIdx)
							{
								//no rollover
								bytestoread = iEndIdx - iStartIdx;
								if(bytestoread > HTTPRCV_RSP_LEN)
								{
									bytestoread = HTTPRCV_RSP_LEN;
								}
								memcpy(pResponse,(const char *) &RX[iStartIdx],bytestoread);
								ret = 0;
							}
							else
							{
								//rollover
								bytestoread = iRxLen - iStartIdx;
								if(bytestoread > HTTPRCV_RSP_LEN)
								{
									bytestoread = HTTPRCV_RSP_LEN;
								}
								memcpy(pResponse,(const char *) &RX[iStartIdx],bytestoread);
								memcpy(&pResponse[bytestoread],(const char *) RX,iEndIdx);
								ret = 0;
							}
						}
					}
				}
				else
				{
					//no start tag found
					pResponse[0] = 0;
				}

				break;
			case ATCMD_CPIN:
				pToken1 = strstr((const char *) RX,"OK");
				if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
				{
				}
				else{
					pToken1 = strstr((const char *) RX,"SIM");
				}
			default:
				break;
		}
	}
	return ret;
}

int searchtoken(char* pToken, char** ppTokenPos)
{
	int  ret = -1;
	char* pToken1 = NULL;

	pToken1 = strstr((const char *) &RX[RXHeadIdx],pToken);
	if(pToken1 != NULL)
	{
		//need to esnure that RXTailIdx is not moving. i.e disable any asynchronous
		//notification
		if (((RXTailIdx > RXHeadIdx) && (pToken1 < &RX[RXTailIdx])) ||
			 (RXTailIdx < RXHeadIdx))
		{
			*ppTokenPos = pToken1;
			ret = 0;
		}
	}
	else if(RXTailIdx < RXHeadIdx)
	{
		//rollover
		pToken1 = strstr((const char *) RX,pToken);
		if((pToken1 != NULL) && (pToken1 < &RX[RXTailIdx]))
		{
			*ppTokenPos = pToken1;
			ret = 0;
		}
		else
		{
		}

	}
	//check http post response is splitted
	else if((RX[iRxLen-2] == pToken[0]) && (RX[iRxLen-1] == pToken[1]) && (RX[0] == pToken[2]))

	{
		*ppTokenPos = (char *) &RX[iRxLen-2];
		ret = 0;
	}
	else if((RX[iRxLen-1] == pToken[0]) && (RX[0] == pToken[1]) && (RX[1] == pToken[2]))
	{
		*ppTokenPos = (char *) &RX[iRxLen-1];
		ret = 0;
	}

	return ret;
}
