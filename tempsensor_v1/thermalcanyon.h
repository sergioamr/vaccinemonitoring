/*
 * thermalcanyon.h
 *
 *  Created on: May 22, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_THERMALCANYON_H_
#define TEMPSENSOR_V1_THERMALCANYON_H_

#include <msp430.h>
#include "stdint.h"

#include "buzzer.h"
#include "config.h"
#include "common.h"
#include "driverlib.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "i2c.h"
#include "timer.h"
#include "modem_uart.h"
#include "battery.h"
#include "rtc.h"
#include "ff.h"
#include "diskio.h"
#include "signal.h"
#include "encode.h"
#include "MMC.h"
#include "pmm.h"
#include "lcd.h"
#include "globals.h"
#include "sms.h"
#include "modem.h"
#include "fatdata.h"
#include "http.h"
#include "stdio.h"
#include "stringutils.h"
#include "temperature.h"
#include "events.h"
#include "time.h"
#include "state_machine.h"
#include "main_system.h"
#include "timer.h"
#include "data_transmit.h"
#include "hardware_buttons.h"

//------------- FUNCTIONS MOVED FROM MAIN - WAITING CLEANUP -------------------

#define FORMAT_FIELD_OFF	1		//2 if field name is 1 character & equal, 3 if field name is 2 character & equal...
extern volatile uint32_t iMinuteTick;

void thermal_canyon_loop();
time_t rtc_update_time();
time_t rtc_get_second_tick();
void thermal_low_battery_hibernate();
uint32_t rtc_get_minute_tick();

#endif /* TEMPSENSOR_V1_THERMALCANYON_H_ */
