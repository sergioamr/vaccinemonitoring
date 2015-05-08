/*
 * uart.c
 *
 *  Created on: Feb 25, 2015
 *      Author: rajeevnx
 */

#include "config.h"
#include "uart.h"
#include "stdlib.h"
#include "string.h"

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

//char Substr[TOKEN_LEN+1];

//local functions
static int searchtoken(char* pToken, char** ppTokenPos);

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

int uart_tx(volatile char* pTxData)
{
	int ret = -1;

	if(pTxData)
	{
		while(iTXInProgress == 1);		//wait till the last msg is transmitted

		iTxLen = strlen(pTxData);
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


	//input check
	if(pResponse)
	{
		switch(atCMD)
		{
			case ATCMD_CCLK:
				pToken1 = strstr(RX,"CCLK:");
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

			break;

			case ATCMD_HTTPSND:
				pToken1 = strstr(&RX[RXHeadIdx],">>>");
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
					pToken1 = strstr(RX,">>>");
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

			case ATCMD_CMGL:
				//check if this msg is unread
				pToken1 = strstr(RX,"UNREAD");
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
								memcpy(pResponse,&RX[iStartIdx],bytestoread);
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
								memcpy(pResponse,&RX[iStartIdx],bytestoread);
								memcpy(&pResponse[bytestoread],RX,iEndIdx);
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

			case ATCMD_HTTPQRY:
				pToken1 = strstr(RX,"HTTPRING");
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
								memcpy(pResponse,&RX[iStartIdx],bytestoread);
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
								memcpy(pResponse,&RX[iStartIdx],bytestoread);
								memcpy(&pResponse[bytestoread],RX,iEndIdx);
								ret = 0;
							}
						}
					}
				}
				break;
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

	pToken1 = strstr(&RX[RXHeadIdx],pToken);
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
		pToken1 = strstr(RX,pToken);
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
		*ppTokenPos = &RX[iRxLen-2];
		ret = 0;
	}
	else if((RX[iRxLen-1] == pToken[0]) && (RX[0] == pToken[1]) && (RX[1] == pToken[2]))
	{
		*ppTokenPos = &RX[iRxLen-1];
		ret = 0;
	}

	return ret;
}
