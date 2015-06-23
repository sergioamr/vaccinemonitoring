/*
 * i2c.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef TEMPSENSOR_V1_I2C_H_
#define TEMPSENSOR_V1_I2C_H_

#include <msp430.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_I2CRX_RETRY  10
//*****************************************************************************
//
//! \brief Initialize I2C.
//!
//! \param speed
//!
//! \return None
//
//*****************************************************************************
extern void i2c_init(uint32_t uiSpeed);

//*****************************************************************************
//
//! \brief Perform I2C read.
//!
//! \param slave address
//! \param command
//! \param data length
//! \param pointer to store the data read from the slave
//!
//! \return None
//
//*****************************************************************************
extern void i2c_read(uint8_t ucSlaveAddr, uint8_t ucCmd, uint8_t ucLen, uint8_t* pucData);

//*****************************************************************************
//
//! \brief Perform I2C write.
//!
//! \param slave address
//! \param command
//! \param data length
//! \param pointer to store the data to be written to slave
//!
//! \return None
//
//*****************************************************************************
extern void i2c_write(uint8_t ucSlaveAddr, uint8_t ucCmd, uint8_t ucLen, uint8_t* pucData);

#ifdef __cplusplus
}
#endif

#endif /* TEMPSENSOR_V1_I2C_H_ */
