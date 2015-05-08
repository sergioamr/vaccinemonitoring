/*
 * battery.c
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#include "config.h"
#include "battery.h"
#include "i2c.h"
#include "timer.h"

#define  TRANS_DELAY 		100

#ifndef BATTERY_DISABLED

void batt_init()
{
	uint8_t 	data;
	uint16_t 	flags = 0;
	uint8_t 	crc = 0;
	uint8_t 	lsb = 0;
	uint8_t 	msb = 0;
	uint8_t		tmp = 0;
	uint16_t 	defcapacity = 0;

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);
	delay(TRANS_DELAY);

	data = 0x80;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);
	delay(TRANS_DELAY);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);
	delay(TRANS_DELAY);

	data = 0x80;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);
	delay(TRANS_DELAY);

	data = 0x13;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);
	delay(TRANS_DELAY);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);
	delay(TRANS_DELAY);

	while(!(flags & 0x10))
	{
		i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 2, &flags);
		delay(TRANS_DELAY);
	}

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CONTROL, 1, &data);
	delay(TRANS_DELAY);

	data = 0x52;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK_CLASS, 1, &data);
	delay(TRANS_DELAY);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DATA_BLOCK, 1, &data);
	delay(TRANS_DELAY);

	i2c_read(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CHECKSUM, 1, &crc);
	delay(TRANS_DELAY);
	i2c_read(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_1, 2, &defcapacity);
	msb = defcapacity & 0xFF;
	lsb = (defcapacity >> 8) & 0xFF;

	data = (BATTERY_CAPACITY >> 8) & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_1, 1, &data);
	delay(TRANS_DELAY);

	data = BATTERY_CAPACITY & 0xFF;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_DESIGN_CAPACITY_2, 1, &data);
	delay(TRANS_DELAY);

	tmp = (255 - crc - lsb - msb) % 256;
	crc = 255 - ((tmp + (BATTERY_CAPACITY & 0xFF) + ((BATTERY_CAPACITY >> 8) & 0xFF00)) % 256);

	i2c_write(SLAVE_ADDR_BATTERY, BATT_BLOCK_DATA_CHECKSUM, 1, &crc);
	delay(TRANS_DELAY);

	data = 0x42;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);
	delay(TRANS_DELAY);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);
	delay(TRANS_DELAY);

	flags = 0;
	while((flags & 0x10))
	{
		i2c_read(SLAVE_ADDR_BATTERY, BATT_FLAGS, 2, &flags);
		delay(TRANS_DELAY);
	}
	data = 0x20;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_1, 1, &data);
	delay(TRANS_DELAY);

	data = 0x00;
	i2c_write(SLAVE_ADDR_BATTERY, BATT_CONTROL_2, 1, &data);
	delay(TRANS_DELAY);

}


int8_t batt_getlevel()
{
	int8_t level = 0;

	i2c_read(SLAVE_ADDR_BATTERY, BATT_STATE_OF_CHARGE, 1, &level);
	return level;
}
#endif
