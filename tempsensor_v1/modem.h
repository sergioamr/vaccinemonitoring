/*
 * modem.h
 *
 *  Created on: May 18, 2015
 *      Author: sergioam
 */

#ifndef MODEM_H_
#define MODEM_H_

extern char ctrlZ[2];
extern char ESC[2];

#ifdef POWER_SAVING_ENABLED
int8_t modem_enter_powersave_mode();
int8_t modem_exit_powersave_mode();
#endif

void modem_init(int8_t slot);

#endif /* MODEM_H_ */
