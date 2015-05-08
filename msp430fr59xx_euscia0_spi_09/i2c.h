/*
 * i2c.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rajeevnx
 */

#ifndef I2C_H_
#define I2C_H_

#include <msp430.h>

#define I2C_POLL 		0x80
#define I2C_INTR 		0x00

#ifdef __cplusplus
extern "C"
{
#endif

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

//*****************************************************************************
//
//! \brief Set polling vs interrupt mode
//!
//! \param mode
//! \return None
//
//*****************************************************************************
extern void i2c_setmode(int8_t mode);

//*****************************************************************************
//
//! \brief Reset mode
//!
//! \param mode
//! \return None
//
//*****************************************************************************
extern void i2c_resetmode(int8_t mode);

//*****************************************************************************
//
//! \brief Disable all i2c interrupts
//!
//! \param None
//! \return None
//
//*****************************************************************************
extern void i2c_disableint();

#ifdef __cplusplus
}
#endif

#endif /* I2C_H_ */

