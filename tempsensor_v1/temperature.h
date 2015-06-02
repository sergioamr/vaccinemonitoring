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

extern void sample_temperature();
extern float resistance_to_temperature(float R);
extern void digital_amp_to_temp_string(int32_t ADCval, char* TemperatureVal, int8_t iSensorIdx);
extern void ADC_setupIO();

#endif /* TEMPERATURE_H_ */
