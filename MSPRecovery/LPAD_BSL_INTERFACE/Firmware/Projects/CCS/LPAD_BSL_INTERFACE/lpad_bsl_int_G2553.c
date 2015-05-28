/* --COPYRIGHT--,BSD
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/**************************************************************************//**
*
* @file     lpad_bsl_int.c
*
* @brief    implementation of MSP430 Launchpad based BSL interface
*
* @version  2.1.0
*
* @author   Leo Hendrawan
*
* @remark   the new MSP430 Launchpad BSL interface software is inspired by
*           the BSL-Bridge implementation by OCY (old_cow_yellow)
*           - thanks OCY! -
*
******************************************************************************/

//*****************************************************************************
// Include section
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <msp430.h>
//#include "msp430g2553.h"
//#include "msp430g2231.h"


//*****************************************************************************
// Global variables
//*****************************************************************************

//*****************************************************************************
// Macros (defines), data types, static variables
//*****************************************************************************

// pin definitions
#define PC_TX_PIN          (BIT1) // P1.1
#define PC_RX_PIN          (BIT2) // P1.2
#define MSP_TX_PIN         (BIT7) // P1.7
#define MSP_RX_PIN         (BIT6) // P1.6
#define MSP_RST_PIN        (BIT4) // P1.4
#define MSP_TEST_PIN       (BIT5) // P1.5
#define MSP_TCK_PIN        (BIT0) // P1.0
#define PB_PIN             (BIT3) // P1.3

// clock setup delay
#define CLK_SETUP_DLY()              __delay_cycles(100)

// PB debouncing delay
#define PB_DEBOUNCING_DLY()          __delay_cycles(400000)

// BSL entry sequence delay
#define BSL_ENTRY_SEQ_LONG_DLY()     __delay_cycles(200)
#define BSL_ENTRY_SEQ_SHORT_DLY()    __delay_cycles(50)

// vlo 1 ms counter variable
uint16_t vlo_1ms_cnt = 0;

// UART parity bit adding variables
static bool add_parity = true;
static uint16_t bit_8_cnt, bit_1_cnt, bit_1_5_cnt, bit_9_cnt;
static uint16_t bit_cnt = 0, ones_cnt = 0;
static bool start_bit = true;
static uint8_t error = 0;

//*****************************************************************************
// Internal function declarations
//*****************************************************************************

static void calibrate_vlo(void);
static void measure_baud_rate(void);
static void delay(uint16_t delay_ms);
static void trap(void);

//*****************************************************************************
// External functions
//*****************************************************************************

/**************************************************************************//**
*
* main
*
* @brief      main function
*
* @param      -
*
* @return     -
*
******************************************************************************/
void main(void)
{
  // Stop WDT
  WDTCTL = WDTPW + WDTHOLD;
  // VLOCLK to source LFXT1S
  BCSCTL3 |= LFXT1S_2;

  // calibrate vlo
  calibrate_vlo();

  // set DCO to work between 8-14 MHz
  DCOCTL = 0x0000;
  BCSCTL1 = XT2OFF | (RSEL3 + RSEL2 + RSEL1);
  DCOCTL = (DCO1 + DCO0);
  
  // wait until clocks stabilize
  do
  {
    BCSCTL3 &= ~(XT2OF + LFXT1OF);
    IFG1 &= ~OFIFG;
    CLK_SETUP_DLY();
  } while(IFG1 & OFIFG);

  // initialize PC & MSP TX&RX pins
  P1DIR |= (MSP_TX_PIN + PC_TX_PIN);  // output pins, default high
  P1OUT |= (MSP_TX_PIN + PC_TX_PIN);
  P1DIR &= ~(MSP_RX_PIN + PC_RX_PIN); // input pins, pull-up enable
  P1REN |= (MSP_RX_PIN + PC_RX_PIN);
  P1OUT |= (MSP_RX_PIN + PC_RX_PIN);

  // initialize MSP RST, TEST, and TCK pins
  P1DIR |= MSP_RST_PIN + MSP_TEST_PIN + MSP_TCK_PIN;
  P1OUT &= ~(MSP_RST_PIN + MSP_TEST_PIN + MSP_TCK_PIN);

  // set push-button as input, pull-up enable
  P1DIR &= ~PB_PIN;
  P1REN |= PB_PIN;
  P1OUT |= PB_PIN;

  // delay for about 1s
  delay(1000);

  // check if push button is pressed
  if(!(P1IN & PB_PIN))
  {
    PB_DEBOUNCING_DLY();
    if(!(P1IN & PB_PIN))
    {
      // sign for not adding parity bit
      add_parity = false;

      // give a red LED blink sign
      P1OUT |= MSP_TCK_PIN;
      delay(500);
      P1OUT &= ~MSP_TCK_PIN;
      delay(500);
    }
  }

  // set up TCK pin as indication of BSL Entry sequence generation
  P1OUT |= MSP_TCK_PIN;

  // generate the BSL entry sequence
  BSL_ENTRY_SEQ_LONG_DLY();
  P1OUT ^= (MSP_TEST_PIN + MSP_TCK_PIN);
  BSL_ENTRY_SEQ_LONG_DLY();
  P1OUT ^= (MSP_TEST_PIN + MSP_TCK_PIN);
  BSL_ENTRY_SEQ_SHORT_DLY();
  P1OUT ^= (MSP_TEST_PIN + MSP_TCK_PIN);
  BSL_ENTRY_SEQ_LONG_DLY();
  P1OUT |= MSP_RST_PIN;
  BSL_ENTRY_SEQ_LONG_DLY();
  P1OUT ^= (MSP_TEST_PIN + MSP_TCK_PIN);

  // check if BSL is invoked
  delay(100);
  if(!(P1IN & MSP_RX_PIN))
  {
	// set error number
	error = 1;

    // trap
    trap();
  }

  if(add_parity == true)
  {
    // run Timer_A in free running mode
    TA0CTL = TASSEL_2 + MC_2 + TACLR;

	// if parity bit shall be added, measure baud rate
    measure_baud_rate();
  }

  // set GPIO interrupts
  P1IES |= (PC_RX_PIN + MSP_RX_PIN);
  P1IE |= (PC_RX_PIN + MSP_RX_PIN);
  P1IFG = 0;

  // ready to go, enable interrupt
  __enable_interrupt();

  while(1); // can also sleep here (but only to LPM0) zzzz...
}

//*****************************************************************************
// Internal functions
//*****************************************************************************

/**************************************************************************//**
*
* calibrate_vlo
*
* @brief         calibrate vlo to get the 1ms counter tick
*
* @param         -
*
* @return        -
*
******************************************************************************/
static void calibrate_vlo(void)
{
  uint16_t temp;

  // check DCO calibrated values
  if((CALDCO_1MHZ != 0xFFFF) && (CALBC1_1MHZ) != 0xFFFF)
  {
	// set DCO with calibrated values
	DCOCTL = CALDCO_1MHZ;
	BCSCTL1 = XT2OFF | CALBC1_1MHZ;

    // wait until clocks stabilize
	do
	{
	  BCSCTL3 &= ~(XT2OF + LFXT1OF);
	  IFG1 &= ~OFIFG;
	  CLK_SETUP_DLY();
	} while(IFG1 & OFIFG);

	// CCR0 works as capture with ACLK as input capture signal
	TA0CCTL0 = CM_1 + CCIS_1 + SCS + CAP;

	// set Timer_A to run in free-running mode and sourced from calibrated SMCLK
	TA0CTL = TASSEL_2 + MC_2 + TACLR;

    // wait for first capture event
	while(!(TA0CCTL0 & CCIFG));

	// clear interrupt
	TA0CCTL0 &= ~CCIFG;

	// read CCR value
	temp = TA0CCR0;

    // wait for second capture event
	while(!(TA0CCTL0 & CCIFG));

	// clear interrupt
	TA0CCTL0 &= ~CCIFG;

	// calculate 1 ms tick
	temp = TA0CCR0 - temp;
	vlo_1ms_cnt = 1000/temp;
	if((1000 % temp) > temp/2)
	{
	  vlo_1ms_cnt++;
	}

	// stop timer
	TA0CTL = 0;
  }
}

/**************************************************************************//**
*
* measure_baud_rate
*
* @brief         measure baud rate
*
* @param         -
*
* @return        -
*
******************************************************************************/
static void measure_baud_rate(void)
{
  uint16_t temp;

  // measure bit time by waiting for the first 0x80 from PC
  // set PC_RX_PIN as capture interrupt
  P1SEL |= PC_RX_PIN;
  TA0CCTL1 = CM_2 + SCS + CAP; // set for capture on falling edge

  // wait until capture event occurs
  while(!(TA0CCTL1 & CCIFG));
  P1OUT &= ~MSP_TX_PIN;

  // clear the interrupt flag and take the CCR value
  TA0CCTL1 &= ~CCIFG;
  temp = TA0CCR1;

  // set PC_RX_PIN to capture the rising next edge of 0x80
  TA0CCTL1 = CM_1 + SCS + CAP;

  // wait until capture event occurs
  while(!(TA0CCTL1 & CCIFG));
  P1OUT |= MSP_TX_PIN;

  // switch off timer capture and calculate the bit time
  TA0CCTL1 = 0;
  bit_8_cnt = TA0CCR1 - temp;
  bit_1_cnt = bit_8_cnt >> 3;
  bit_1_5_cnt = (bit_8_cnt >> 4) + bit_1_cnt;
  bit_9_cnt = bit_8_cnt + bit_1_cnt + (bit_1_cnt >> 4);

  // set back PC_RX_PIN as GPIO
  P1SEL &= ~PC_RX_PIN;
}

/**************************************************************************//**
*
* delay
*
* @brief         a very rough delay using VLOCLK, only to be used during init
*
* @param         delay_ms    delay time in miliseconds
*
* @return        -
*
******************************************************************************/
static void delay(uint16_t delay_ms)
{
  // set register
  TA0CCTL0 = 0;
  if(vlo_1ms_cnt == 0)
  {
	// use typical VLO clock 12 kHz
	TA0CCR0 = (12000/1000) * delay_ms;
  }
  else
  {
	// use the calibrated value
	TA0CCR0 =  vlo_1ms_cnt * delay_ms;
  }

  // start timer
  TA0CTL = TASSEL_1 + MC_1 + TACLR;

  // wait until flag is set
  while(!(TA0CCTL0 & CCIFG));

  // reset register
  TA0CTL = 0;
}

/**************************************************************************//**
*
* trap
*
* @brief         trap the whole system - assert like function
*
* @param         -
*
* @return        -
*
******************************************************************************/
static void trap(void)
{
  uint16_t delay_blink;

  // disable interrupt
  __disable_interrupt();

  // set LED pins as output
  P1REN = 0;
  P1DIR = MSP_RX_PIN + MSP_TCK_PIN;
  P1OUT = MSP_RX_PIN;

  // set delay blink according to the error number
  delay_blink = 100 * error;

  while(1)
  {
    // blink, blink, blink till the end of time...
	P1OUT ^= MSP_RX_PIN + MSP_TCK_PIN;
	delay(delay_blink);
  }
}

/**************************************************************************//**
*
* Port1_Isr
*
* @brief         Port 1 interrupt
*
* @param         -
*
* @return        -
*
******************************************************************************/
#pragma vector=PORT1_VECTOR
__interrupt void Port1_Isr(void)
{
  // MSP_RX_PIN state change
  if(P1IFG & MSP_RX_PIN)
  {
    // change output pin
    P1OUT ^= PC_TX_PIN;

    // clear interrupt and change interrupt transition
    P1IFG &= ~MSP_RX_PIN;
    P1IES ^= MSP_RX_PIN;
  }
  // PC_RX_PIN state change
  else if(P1IFG & PC_RX_PIN)
  {
    // change output pin
    P1OUT ^= MSP_TX_PIN;

    // clear interrupt and change interrupt transition
    P1IFG &= ~PC_RX_PIN;
    P1IES ^= PC_RX_PIN;

    // activate timers if necessary
    if((add_parity == true) && (start_bit == true))
    {
      // toggle flag
      start_bit = false;

      // CCR0 is used for sampling the bits
      TA0CCR0 = TA0R + bit_1_5_cnt;
      TA0CCTL0 = CCIE;

      // CCR1 is used to mark the end of the bits
      TA0CCR1 = TA0R + bit_9_cnt;
      TA0CCTL1 = CCIE;
    }
  }
}

/**************************************************************************//**
*
* TimerA_Ccr0_Isr
*
* @brief         Timer_A CCR0 interrupt
*
* @param         -
*
* @return        -
*
******************************************************************************/
#pragma vector=TIMER0_A0_VECTOR        //TIMERA0_VECTOR
__interrupt void TimerA_Ccr0_Isr(void)
{
  // clear interrupt
  TA0CCTL0 &= ~CCIFG;

  // check if the ISR is actually valid
  if(add_parity == false)
  {
	// set error variable
	error = 2;

	// trap CPU
	trap();
  }

  // sample one bits
  if(P1IN & PC_RX_PIN)
  {
    ones_cnt++;
  }

  // check if already 8 bits sampled
  if(++bit_cnt < 8)
  {
    // re-trigger in 1 bit time
    TA0CCR0 = TA0R + bit_1_cnt;
  }

  if(bit_cnt == 8)
  {
    // stop timer
    TA0CCTL0 = 0;
  }
}

/**************************************************************************//**
*
* TimerA_Vect_Isr
*
* @brief         Timer_A vector interrupt
*
* @param         -
*
* @return        -
*
******************************************************************************/
#pragma vector=TIMER0_A1_VECTOR 		//TIMERA1_VECTOR
__interrupt void TimerA_Vect_Isr(void)
{
  static bool start_parity = true;
  bool reset = true;

  // check if the ISR is actually valid
  if(add_parity == false)
  {
	// set error variable
	error = 3;

	// trap CPU
	trap();
  }

  if(TA0IV == 0x02)
  {
    // clear interrupt flag
    TA0CCTL1 &= ~CCIFG;

    if(start_parity == true)
    {
      // check the ones bit
      if(!(ones_cnt & 0x01))
      {
        // if 1s are even, set parity bit to 0
        P1OUT &= ~MSP_TX_PIN;

        // start timer
        TA0CCR1 = TA0R + bit_1_cnt;

        // toggle flag
        start_parity = false;

        // do not reset
        reset = false;
      }
    }
    else
    {
      // toggle output after parity bit
      P1OUT |= MSP_TX_PIN;

      // toggle flag
      start_parity = true;
    }

    if(reset == true)
    {
      // reset counters
      bit_cnt = 0;
      ones_cnt = 0;

      // switch off CCR
      TA0CCTL1 = 0;

      // set flag for start bit
      start_bit = true;
    }
  }
}

/**************************************************************************//**
*
* Trap_Isr
*
* @brief         trap ISR for unwanted interrupt
*
* @param         -
*
* @return        -
*
******************************************************************************/
#pragma vector=NMI_VECTOR,ADC10_VECTOR,PORT2_VECTOR,USCIAB0TX_VECTOR,USCIAB0RX_VECTOR,WDT_VECTOR
__interrupt void Trap_Isr(void)
{
  // set error number
  error = 4;

  // trap CPU
  trap();
}
