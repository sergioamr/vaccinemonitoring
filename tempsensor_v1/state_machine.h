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
#define STATE_OFF 0
#define STATE_ON 1

SYSTEM_STATUS *state_getAlarms();

void state_reset_sensor_alarm(int c);
void state_init();
void state_power_on();
void state_power_out();
uint8_t state_isBuzzerOn();

void state_network_fail(uint8_t sim, uint16_t error);
void state_network_success(uint8_t sim);

void state_modem_timeout(uint8_t sim);

void state_failed_sdcard(uint16_t error);

void state_failed_gprs(uint8_t sim);
void state_failed_gsm(uint8_t sim);

void state_process();
void state_sensor_temperature(uint8_t sensor, float temp);
void state_battery_level(uint8_t battery_level);
void state_check_power();

#endif /* STATE_MACHINE_H_ */
