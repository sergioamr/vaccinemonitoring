/*
 * calibration.c
 *
 *  Created on: May 20, 2015
 *      Author: sergioam
 */

#include "stdint.h"
#include <msp430.h>
#include "i2c.h"
#include "timer.h"
#include "stdlib.h"
#include "uart.h"
#include "config.h"
#include "framctl.h"
#include "battery.h"
#include "stringutils.h"
#include "common.h"
#include "driverlib.h"
#include "globals.h"
#include "lcd.h"
#include "string.h"
#include "stdio.h"
#include "modem.h"
#include "sms.h"

#define  CALIBRATE 4	//0 - default, 1 = device A, 2 = device B...

#include <msp430.h>

int main_calibration(void) {

#if CALIBRATE == 1
	//device A, sensor A
	g_pInfoB->calibration[0][0]= -0.0089997215012;
	g_pInfoB->calibration[0][1]= 1.0168558678;

	//device A, sensor B
	g_pInfoB->calibration[1][0]= -0.1246539991;
	g_pInfoB->calibration[1][1]= 1.0160181876;

	//device A, sensor C
	g_pInfoB->calibration[2][0]= 0.0113285399;
	g_pInfoB->calibration[2][1]= 1.0181842794;

	//device A, sensor D
	g_pInfoB->calibration[3][0]= -0.2116162843;
	g_pInfoB->calibration[3][1]= 1.0142320871;

	//device A, sensor E
	g_pInfoB->calibration[4][0]= -0.1707975911;
	g_pInfoB->calibration[4][1]= 1.018139702;

#elif CALIBRATE == 2
	//device B, sensor A
	g_pInfoB->calibration[0][0]= -0.11755709;
	g_pInfoB->calibration[0][1]= 1.0126639507;

	//device B, sensor B
	g_pInfoB->calibration[1][0]= -0.1325491351;
	g_pInfoB->calibration[1][1]= 1.0130573199;

	//device B, sensor C
	g_pInfoB->calibration[2][0]= 0.0171280734;
	g_pInfoB->calibration[2][1]= 1.0133498078;

	//device B, sensor D
	g_pInfoB->calibration[3][0]= -0.180533434;
	g_pInfoB->calibration[3][1]= 1.0138440268;

	//device B, sensor E
	g_pInfoB->calibration[4][0]= -0.0898083156;
	g_pInfoB->calibration[4][1]= 1.017814462;

#elif CALIBRATE == 3
	//device C, sensor A
	g_pInfoB->calibration[0][0]= -0.2848321;
	g_pInfoB->calibration[0][1]= 1.021466371;

	//device C, sensor B
	g_pInfoB->calibration[1][0]= -0.289122994;
	g_pInfoB->calibration[1][1]= 1.022946138;

	//device C, sensor C
	g_pInfoB->calibration[2][0]= -0.208889079;
	g_pInfoB->calibration[2][1]= 1.02581601;

	//device C, sensor D
	g_pInfoB->calibration[3][0]= -0.28968586;
	g_pInfoB->calibration[3][1]= 1.023883188;

	//device C, sensor E
	g_pInfoB->calibration[4][0]= -0.0765155;
	g_pInfoB->calibration[4][1]= 1.021308586;

#elif CALIBRATE == 4
	//device D, sensor A
	g_pInfoB->calibration[0][0] = 0.15588;
	g_pInfoB->calibration[0][1] = 1.013487;

	//device D, sensor B
	g_pInfoB->calibration[1][0] = -0.09636;
	g_pInfoB->calibration[1][1] = 1.011564;

	//device D, sensor C
	g_pInfoB->calibration[2][0] = -0.07391;
	g_pInfoB->calibration[2][1] = 1.010255;

	//device D, sensor D
	g_pInfoB->calibration[3][0] = 0.190958;
	g_pInfoB->calibration[3][1] = 1.017441;

	//device D, sensor E
	g_pInfoB->calibration[4][0] = 0.11215;
	g_pInfoB->calibration[4][1] = 1.010953;

#endif

#ifdef LCD_TEST
	//test the LCD
	memset(TMP, 0, sizeof(TMP));
	lcd_reset();
	lcd_blenable();
	g_iLCDVerbose = VERBOSE_BOOTING;
	lcd_init();

	__bis_SR_register(GIE);

	delay(1000);
	lcd_clear();
	lcd_print_lne("CALIBRATION MODE", LINE1);
	delay(1000);
	lcd_print_lne("V(" __DATE__ ")",LINE2);
	delay(2000);

	i2c_init(380000);

#endif
	return 1;
}
