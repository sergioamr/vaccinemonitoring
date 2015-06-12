/*
 * state_machine.h
 *
 *  Created on: Jun 12, 2015
 *      Author: sergioam
 */

#ifndef STATE_MACHINE_H_
#define STATE_MACHINE_H_

extern SYSTEM_STATE *g_pSysState;

#define STATE_CONNECTION 1

void state_network_fail(uint8_t sim, uint16_t error);
void state_network_success(uint8_t sim);

void state_modem_timeout(uint8_t sim);

void state_failed_sdcard(uint16_t error);

void state_failed_gprs(uint8_t sim);
void state_failed_gsm(uint8_t sim);

void state_process();
void state_sensor_temperature(uint8_t sensor, float temp);

#endif /* STATE_MACHINE_H_ */
