/*
 * temperature.h
 *
 *  Created on: 2 Jun 2015
 *      Author: dancat
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#define USE_TEMPERATURE TEMPERATURE *tem = &g_pSysState->temp;

void temperature_analog_to_digital_conversion();
void temperature_subsamples(uint8_t samples); // Captures N subsamples
void temperature_trigger_init();
void temperature_trigger_capture();
void temperature_single_capture();

char *temperature_getString(uint8_t id);
float resistance_to_temperature(float R);

void temperature_process_ADC_values();
void ADC_setupIO();
char *temperature_getString(uint8_t id);
uint32_t temperature_getValueInt(uint8_t id);
float temperature_getValueFloat(uint8_t id);

#endif /* TEMPERATURE_H_ */
