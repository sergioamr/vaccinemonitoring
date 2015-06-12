/* Copyright (c) 2015, Intel Corporation. All rights reserved.
 *
 * INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL� PRODUCTS. NO LICENSE, EXPRESS OR IMPLIED,
 * BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS IS GRANTED BY THIS DOCUMENT. EXCEPT AS PROVIDED
 * IN INTEL'S TERMS AND CONDITIONS OF SALE FOR SUCH PRODUCTS, INTEL ASSUMES NO LIABILITY WHATSOEVER, AND INTEL
 * DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY, RELATING TO SALE AND/OR USE OF INTEL PRODUCTS INCLUDING LIABILITY
 * OR WARRANTIES RELATING TO FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABILITY, OR INFRINGEMENT OF ANY PATENT,
 * COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.
 * UNLESS OTHERWISE AGREED IN WRITING BY INTEL, THE INTEL PRODUCTS ARE NOT DESIGNED NOR INTENDED FOR ANY APPLICATION
 * IN WHICH THE FAILURE OF THE INTEL PRODUCT COULD CREATE A SITUATION WHERE PERSONAL INJURY OR DEATH MAY OCCUR.
 *
 * Intel may make changes to specifications and product descriptions at any time, without notice.
 * Designers must not rely on the absence or characteristics of any features or instructions marked
 * "reserved" or "undefined." Intel reserves these for future definition and shall have no responsibility
 * whatsoever for conflicts or incompatibilities arising from future changes to them. The information here
 * is subject to change without notice. Do not finalize a design with this information.
 * The products described in this document may contain design defects or errors known as errata which may
 * cause the product to deviate from published specifications. Current characterized errata are available on request.
 *
 * This document contains information on products in the design phase of development.
 * All Thermal Canyons featured are used internally within Intel to identify products
 * that are in development and not yet publicly announced for release.  Customers, licensees
 * and other third parties are not authorized by Intel to use Thermal Canyons in advertising,
 * promotion or marketing of any product or services and any such use of Intel's internal
 * Thermal Canyons is at the sole risk of the user.
 *
 */

#include "thermalcanyon.h"
#include "data_transmit.h"
#include "hardware_buttons.h"
#include "fatdata.h"
#include "events.h"

// Overall state of the device to take decisions upon the state of the modem, storage, alerts, etc.

#pragma SET_DATA_SECTION(".state_machine")
SYSTEM_STATE g_SystemState;	// system states to control errors, connectivity, retries, etc
#pragma SET_DATA_SECTION()

SYSTEM_STATE *g_pSysState = &g_SystemState;

/***********************************************************************************************************************/
/* GENERATE ALARMS */
/***********************************************************************************************************************/

void state_alert_on() {

}

/***********************************************************************************************************************/
/* MODEM & COMMUNICATIONS */
/***********************************************************************************************************************/

// Clear all the errors for the network connection.
void state_network_success(uint8_t sim) {
	SIM_STATE *simState = &g_pSysState->simState[sim];

	// Eveything is fine
	simState->failsGPRS = 0;
	simState->failsGSM = 0;
	simState->modemErrors = 0;
}

// Checks several parameters to see if we have to reset the modem, switch sim card, etc.
void state_network_fail(uint8_t sim, uint16_t error) {

}

void state_modem_timeout(uint8_t sim) {

}

void state_failed_gprs(uint8_t sim) {
	SIM_STATE *simState = &g_pSysState->simState[sim];
	simState->failsGSM++;
}

void state_failed_gsm(uint8_t sim) {
	SIM_STATE *simState = &g_pSysState->simState[sim];
	simState->failsGPRS++;
}

/***********************************************************************************************************************/
/* STORAGE */
/***********************************************************************************************************************/

void state_failed_sdcard(uint16_t error) {

}

/***********************************************************************************************************************/
/* TEMPERATURE CHECKS */
/***********************************************************************************************************************/

void state_sensor_temperature(uint8_t sensor, float temp) {

}

/***********************************************************************************************************************/
/* BATTERY CHECKS */
/***********************************************************************************************************************/

void state_battery_level(uint8_t battery_level) {

}

/***********************************************************************************************************************/
/* POWER CHECKS */
/***********************************************************************************************************************/

// Power out, we store the time in which the power went down
void state_power_out() {
	g_pSysState->power = STATE_POWER_OFF;
	if (g_pSysState->time_powerOutage!=0)
		g_pSysState->time_powerOutage = thermal_update_time();

	// TODO Check the power down time to generate an alert
}

void state_power_on() {
	g_pSysState->power = STATE_POWER_ON;
	g_pSysState->time_powerOutage = 0;
}

void state_check_power() {
	if (!(P4IN & BIT4))
		state_power_on();
	else
		state_power_off();
}

/***********************************************************************************************************************/
/* Check the state of every component */
/***********************************************************************************************************************/

void state_process() {
	state_check_power();
}
