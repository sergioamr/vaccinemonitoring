/*
 * watchdog.c
 *
 *  Created on: Jun 17, 2015
 *      Author: sergioam
 */
#include "thermalcanyon.h"
#include "wdt_a.h"

void watchdog_disable() {
	WDTCTL = WDTPW | WDTHOLD;                 // Stop WDT
}

void watchdog_init() {
/*

    //Watchdog mode -> reset after expired time
    //WDT is clocked by VLOCLK since ALCK is failed without initialization
    //Set Watchdog Timer timeout 3s - SET BREAKPOINT HERE
	WDT_A_watchdogTimerInit(WDT_A_BASE,
                            WDT_A_CLOCKSOURCE_ACLK,
                            WDT_A_CLOCKDIVIDER_32K);

    WDT_A_start(WDT_A_BASE);
*/
}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void) {
	_NOP();
	//WDT_A_resetTimer(WDT_A_BASE);
}

