/*
 * battery.c
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#include "thermalcanyon.h"
#include "config.h"
#include "battery.h"
#include "i2c.h"
#include "timer.h"
#include "lcd.h"
#include "state_machine.h"

#define  TRANS_DELAY 		10

#ifndef BATTERY_DISABLED


int16_t g_iFullRecharge = 0;

int8_t batt_isPlugged() {
	if (g_pSysState->battery_level==0 || g_pSysState->battery_level>100)
		return 0;

	return 1;
}

uint8_t batt_check_level() {
	uint8_t iBatteryLevel = 0;
	// Battery checks
	lcd_print("Battery check");

#ifndef BATTERY_DISABLED
	iBatteryLevel = batt_getlevel();
#endif

	if (iBatteryLevel == 0)
		lcd_printl(LINE2, "Battery FAIL");
	else if (iBatteryLevel > 100)
		lcd_printl(LINE2, "Battery UNKNOWN");
	else if (iBatteryLevel > 99)
		lcd_printl(LINE2, "Battery FULL");
	else if (iBatteryLevel > 15)
		lcd_printl(LINE2, "Battery OK");
	else if (iBatteryLevel)
		lcd_printl(LINE2, "Battery LOW");

	if (iBatteryLevel<15 || iBatteryLevel>100)
		delay(10000); // Delay to display that there is a state to show

	return iBatteryLevel;
}

void batt_init()
{
	uint16_t 	flags = 0;
	uint16_t 	designenergy = 0;
	uint16_t 	terminalvolt = 0;
	uint16_t 	taperrate = 0;
	uint16_t 	tapervolt = 0;
	uint8_t 	data;
	uint8_t 	crc = 0;
	uint8_t 	lsb = 0;
	uint8_t 	msb = 0;
	uint8_t		tmp = 0;
	uint8_t 	dummy1;
	uint8_t 	dummy2;

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);

	data = 0x80;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);

	data = 0x80;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);

	data = 0x13;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);

	while(!(flags & 0x10))
	{
		//i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 2, &flags);
		i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 1, (uint8_t *) &flags);
	}

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CONTROL, 1, &data);

	data = 0x52;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK_CLASS, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK, 1, &data);

	i2c_read(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CHECKSUM, 1, &crc);

	//design capacity
	//i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_1, 2, &defcapacity);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_1, 1, &msb);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_2, 1, &lsb);

	data = (BATTERY_CAPACITY >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_1, 1, &data);

	data = BATTERY_CAPACITY & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_2, 1, &data);

	//design energy
	//i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_ENERGY_1, 2, &designenergy);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_ENERGY_1, 1, &dummy1);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_ENERGY_2, 1, &dummy2);
	designenergy = (dummy1 << 8) | dummy2;

	data = (DESIGN_ENERGY >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_ENERGY_1, 1, &data);

	data = DESIGN_ENERGY & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_ENERGY_2, 1, &data);

	//terminal voltage
	//i2c_read(SLAVE_ADDR_BATTERY, BATT_TERM_VOLT_1, 2, &terminalvolt);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TERM_VOLT_1, 1, &dummy1);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TERM_VOLT_2, 1, &dummy2);
	terminalvolt = (dummy1 << 8) | dummy2;

	data = (TERMINAL_VOLTAGE >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TERM_VOLT_1, 1, &data);

	data = TERMINAL_VOLTAGE & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TERM_VOLT_2, 1, &data);

	//taper rate
	//i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_1, 2, &taperrate);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_1, 1, &dummy1);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_2, 1, &dummy2);
	taperrate = (dummy1 << 8) | dummy2;

	data = (TAPER_RATE >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_1, 1, &data);

	data = TAPER_RATE & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_2, 1, &data);

	//taper volt
	//i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_RATE_1, 2, &taperrate);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_VOLT_1, 1, &dummy1);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_TAPER_VOLT_2, 1, &dummy2);
	tapervolt = (dummy1 << 8) | dummy2;

	data = (TAPER_VOLT >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TAPER_VOLT_1, 1, &data);

	data = TAPER_VOLT & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_TAPER_VOLT_2, 1, &data);

	tmp = (255 - crc - lsb - msb -(designenergy & 0xFF) - ((designenergy >> 8) & 0xFF)
			-(terminalvolt & 0xFF) - ((terminalvolt >> 8) & 0xFF)
			-(tapervolt & 0xFF) - ((tapervolt >> 8) & 0xFF)
			-(taperrate & 0xFF) - ((taperrate >> 8) & 0xFF) ) % 256;

	crc = 255 - ((tmp + (BATTERY_CAPACITY & 0xFF) + ((BATTERY_CAPACITY >> 8) & 0x00FF)
			  	  + (DESIGN_ENERGY & 0xFF) + ((DESIGN_ENERGY >> 8) & 0x00FF)
				  + (TERMINAL_VOLTAGE & 0xFF) + ((TERMINAL_VOLTAGE >> 8) & 0x00FF)
				  + (TAPER_RATE & 0xFF) + ((TAPER_RATE >> 8) & 0x00FF)
				  + (TAPER_VOLT & 0xFF) + ((TAPER_VOLT >> 8) & 0x00FF)
				 ) % 256);

	i2c_write(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CHECKSUM, 1, &crc);

	data = 0x42;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);

	flags = 0;
	while((flags & 0x10))
	{
		//i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 2, &flags);
		i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 1, (uint8_t *) &flags);
	}
	data = 0x20;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);

#if 0
//calibration experiment

	data = 0x59;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK_CLASS, 1, &data);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK, 1, &data);

	iIdx = 0;
	while (iIdx < 28)
	{
		i2c_read(SLAVE_ADDR_BATTERY, iIdx, 5, &reg[iIdx]);
		iIdx += 5;
	}
#endif

	delay(1000);
}


uint8_t batt_getlevel()
{
	// Read battery levels only in minutes
	static uint32_t lastTick = UINT32_MAX;
	if (g_pSysState->battery_level!=0 && lastTick==rtc_get_minute_tick())
		return g_pSysState->battery_level;

	lastTick = rtc_get_minute_tick();
	uint8_t level = 0;
	double adjustedlevel = 0.0;

	i2c_read(SLAVE_ADDR_BATTERY, BATT_STATE_OF_CHARGE, 1, (uint8_t *) &level);

	adjustedlevel = level*1.15;
	if(adjustedlevel > 100.0)
	{
		level = 100;
	}
	else
	{
		level = (int8_t)adjustedlevel;
	}

	g_pSysState->battery_level = level;
	return level;
}
#endif
