/*
 * battery.h
 *
 *  Created on: Feb 11, 2015
 *      Author: rajeevnx
 */

#ifndef BATTERY_H_
#define BATTERY_H_

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
//*****************************************************************************
//
//! \brief Initialize Battery fuel guage.
//!
//! \param speed
//!
//! \return None
//
//*****************************************************************************
extern void batt_init();

//*****************************************************************************
//
//! \brief Get the battery remaining capacity
//!
//!
//! \return battery remaining capacity in percentage
//
//*****************************************************************************
extern int8_t batt_getlevel();

#ifdef __cplusplus
}
#endif



#endif /* BATTERY_H_ */
