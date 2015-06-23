/*
 * battery.h
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_BATTERY_H_
#define TEMPSENSOR_V1_BATTERY_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define BATT_CONTROL_1				0x00
#define BATT_CONTROL_2				0x01
#define BATT_FLAGS					0x06
#define BATT_FULL_AVAIL_CAPACITY	0x0a
#define BATT_REMAINING_CAPACITY		0x0c
#define BATT_FULL_CHG_CAPACITY		0x0e
#define BATT_STATE_OF_CHARGE		0x1c
#define BATT_BLOCK_DATA_CHECKSUM	0x60
#define BATT_BLOCK_DATA_CONTROL		0x61
#define BATT_DATA_BLOCK_CLASS		0x3E
#define BATT_DATA_BLOCK				0x3F
#define BATT_DESIGN_CAPACITY_1		0x4A
#define BATT_DESIGN_CAPACITY_2		0x4B
#define BATT_DESIGN_ENERGY_1		0x4C
#define BATT_DESIGN_ENERGY_2		0x4D
#define BATT_TERM_VOLT_1			0x50
#define BATT_TERM_VOLT_2			0x51
#define BATT_TAPER_RATE_1			0x5B
#define BATT_TAPER_RATE_2			0x5C
#define BATT_TAPER_VOLT_1			0x5D
#define BATT_TAPER_VOLT_2			0x5E
#define BATT_FULLCHRGCAP			0x0E

//*****************************************************************************
//! \brief Initialize Battery fuel guage.
//! \param speed
//! \return None
//*****************************************************************************
void batt_init();

//*****************************************************************************
//! \brief Get the battery remaining capacity
//! \return battery remaining capacity in percentage
//*****************************************************************************
uint8_t batt_getlevel();

// Checks level, displays a message with the result
uint8_t batt_check_level();
int8_t batt_isPlugged();

#ifdef __cplusplus
}
#endif



#endif /* TEMPSENSOR_V1_BATTERY_H_ */
