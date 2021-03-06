/*
 * hardware_buttons.h
 *
 *  Created on: Jun 1, 2015
 *      Author: sergioam
 */

#ifndef HARDWARE_BUTTONS_H_
#define HARDWARE_BUTTONS_H_

#define MODEM_ON !(P4IN & BIT0)
#define POWER_ON !(P4IN & BIT4)

void switchers_setupIO();
uint8_t switch_check_service_pressed();

void hardware_disable_buttons();
void hardware_enable_buttons();
void hardware_actions();

#endif /* HARDWARE_BUTTONS_H_ */
