/*
 * temperature.h
 *
 *  Created on: 2 Jun 2015
 *      Author: dancat
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

extern volatile char g_isConversionDone;
extern volatile int g_iSamplesRead;

extern char g_conversionTriggered;

void temperature_sample();
float resistance_to_temperature(float R);
void digital_amp_to_temp_string(int32_t ADCval, char* TemperatureVal, int8_t iSensorIdx);
void temperature_process_ADC_values();
void ADC_setupIO();

#endif /* TEMPERATURE_H_ */
