/*
 * util.h
 *
 *  Created on: Mar 16, 2015
 *      Author: rajeevnx
 */

#ifndef UTIL_H_
#define UTIL_H_

//*****************************************************************************
//
//! \brief Convert integer to string with no padding
//!
//! \param integer
//!
//! \return none
//
//*****************************************************************************
char* itoa_nopadding(int num);

//*****************************************************************************
//
//! \brief Convert integer to string with a leading zero for single digit
//!
//! \param integer
//!
//! \return none
//
//*****************************************************************************
char* itoa(int num);

#endif /* UTIL_H_ */
