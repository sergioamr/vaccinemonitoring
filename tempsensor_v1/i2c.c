/*
 * i2c.c
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

//macros for cMode
#define I2C_WRITE				0x00
#define I2C_READ				0x01
#define I2C_DATA_RECEIVED 		0x02

#include "config.h"
#include "i2c.h"
#include "driverlib.h"
#include "timer.h"

#pragma SET_DATA_SECTION(".aggregate_vars")
volatile int8_t 	I2CRX[I2C_RX_LEN];
volatile int8_t 	I2CTX[I2C_TX_LEN];
volatile int8_t  	iI2CRxIdx = 0;
volatile int8_t    iI2CTxIdx = 0;
volatile int8_t    iI2CTxLen = 0;
volatile int8_t    iI2CRxLen = 0;
volatile int8_t     cMode  = 0;
#pragma SET_DATA_SECTION()

void i2c_init(uint32_t uiSpeed)
{
	  UCB0CTLW0 |= UCSWRST;                     				// Software reset enabled
	  UCB0CTLW0 |= UCMODE_3 | UCMST | UCSYNC | UCSSEL__SMCLK;   // I2C mode, Master mode, sync, SMCLK src
	  UCB0BRW 	= (uint16_t) (CS_getSMCLK() / uiSpeed);
	  UCB0CTLW0 &= ~UCSWRST;                     				// enable I2C by releasing the reset
	  UCB0IE |= UCRXIE0 | UCTXIE0 | UCSTTIE | UCNACKIE;					// enable Rx and Tx interrupt
}

void i2c_read(uint8_t ucSlaveAddr, uint8_t ucCmd, uint8_t ucLen, uint8_t* pucData)
{
	 int8_t      iRetry = MAX_I2CRX_RETRY;
	 UCB0I2CSA = ucSlaveAddr;                  				// Slave address
	 cMode = I2C_READ;
	 I2CTX[0] = ucCmd;
	 iI2CTxIdx = iI2CRxIdx =0;
	 iI2CTxLen = 1;
	 iI2CRxLen = ucLen;

	 UCB0CTL1 |= UCTR;
	 UCB0CTL1 |= UCTXSTT;                    // I2C start condition

	 while(!(cMode & I2C_DATA_RECEIVED) && iRetry){ iRetry--;
#ifndef _DEBUG
	 	 delay(100);
#else
	 	delay(10);
#endif
	 	 pucData[0] = 0;
	 };
	 memcpy((void *) pucData,(const void *) I2CRX,ucLen);
}

void i2c_write(uint8_t ucSlaveAddr, uint8_t ucCmd, uint8_t ucLen, uint8_t* pucData)
{
	 UCB0I2CSA = ucSlaveAddr;                  				// Slave address
	 cMode = I2C_WRITE;
	 I2CTX[0] = ucCmd;
	 memcpy((char *) &I2CTX[1],(const char *) pucData,ucLen);
	 iI2CTxIdx = 0;
	 iI2CTxLen = ucLen+1;

	 UCB0CTL1 |= UCTR;
	 UCB0CTL1 |= UCTXSTT;                    // I2C start condition

}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_B0_VECTOR))) USCIB0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	  switch(__even_in_range(UCB0IV, USCI_I2C_UCBIT9IFG))
	  {
	    case USCI_NONE:          break;         // Vector 0: No interrupts
	    case USCI_I2C_UCALIFG:   break;         // Vector 2: ALIFG
	    case USCI_I2C_UCNACKIFG:                // Vector 4: NACKIFG
	      UCB0CTL1 |= UCTXSTT;                  // I2C start condition
	      break;
	    case USCI_I2C_UCSTTIFG:
	      break;         // Vector 6: STTIFG
	    case USCI_I2C_UCSTPIFG:
	      break;         // Vector 8: STPIFG
	    case USCI_I2C_UCRXIFG3:  break;         // Vector 10: RXIFG3
	    case USCI_I2C_UCTXIFG3:  break;         // Vector 12: TXIFG3
	    case USCI_I2C_UCRXIFG2:  break;         // Vector 14: RXIFG2
	    case USCI_I2C_UCTXIFG2:  break;         // Vector 16: TXIFG2
	    case USCI_I2C_UCRXIFG1:  break;         // Vector 18: RXIFG1
	    case USCI_I2C_UCTXIFG1:  break;         // Vector 20: TXIFG1
	    case USCI_I2C_UCRXIFG0:                 // Vector 22: RXIFG0
	      I2CRX[iI2CRxIdx] = UCB0RXBUF;                   // Get RX data
	      if ((cMode & I2C_READ) && (iI2CRxIdx == iI2CRxLen))
	      {
    		  //trigger a stop condition
    		  UCB0CTL1 |= UCTXSTP;
    		  cMode |= I2C_DATA_RECEIVED;
	      }
	      //update the RX index
	      iI2CRxIdx = (iI2CRxIdx + 1);
	      if(iI2CRxIdx >= I2C_RX_LEN)
	      {
	    	  iI2CRxIdx = 0;
	      }
	      //__bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
	      break;
	    case USCI_I2C_UCTXIFG0:  		        // Vector 24: TXIFG0
	      if(iI2CTxIdx < iI2CTxLen)
	      {
	    	  UCB0TXBUF = I2CTX[iI2CTxIdx];
	    	  iI2CTxIdx = (iI2CTxIdx + 1);
	      }
	      else
	      {
	    	  if(cMode & I2C_READ)
	    	  {
	    		  //trigger a repeat start with receiver mode
	    		  UCB0CTL1 &= ~UCTR;
	    		  UCB0CTL1 |= UCTXSTT;                    // I2C start condition
	    	  }
	    	  else
	    	  {
	    		  //trigger a stop condition
	    		  UCB0CTL1 |= UCTXSTP;
	    	  }
	      }
	      break;
	    case USCI_I2C_UCBCNTIFG:                // Vector 26: BCNTIFG
	      break;
	    case USCI_I2C_UCCLTOIFG: break;         // Vector 28: clock low timeout
	    case USCI_I2C_UCBIT9IFG: break;         // Vector 30: 9th bit
	    default: break;
	  }
}
