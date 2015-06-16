/*
 * temperature.c
 *
 *  Created on: 2 Jun 2015
 *      Author: dancat
 */

#include "msp430.h"
#include "config.h"
#include "common.h"
#include "globals.h"
#include "temperature.h"
#include "math.h"
#include "stdio.h"
#include "string.h"
#include "events.h"

void ADC_setupIO() {

	P1SELC |= BIT0 | BIT1;                    // Enable VEREF-,+
	P1SELC |= BIT2;                           // Enable A/D channel A2
#ifdef SEQUENCE
	P1SELC |= BIT3 | BIT4 | BIT5;          // Enable A/D channel A3-A5
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	P4SELC |= BIT2;          				// Enable A/D channel A10
#endif
#endif

#ifdef POWER_SAVING_ENABLED
	P3DIR |= BIT3;							// Modem DTR
	P3OUT &= ~BIT3;// DTR ON (low)
#endif
	//P1SEL0 |= BIT2;

	// Configure ADC12
	ADC12CTL0 = ADC12ON | ADC12SHT0_2;       // Turn on ADC12, set sampling time
	ADC12CTL1 = ADC12SHP;                     // Use pulse mode
	ADC12CTL2 = ADC12RES_2;                   // 12bit resolution
	ADC12CTL3 |= ADC12CSTARTADD_2;			// A2 start channel
	ADC12MCTL2 = ADC12VRSEL_4 | ADC12INCH_2; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 2
#ifdef SEQUENCE
	ADC12MCTL3 = ADC12VRSEL_4 | ADC12INCH_3; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 3
	ADC12MCTL4 = ADC12VRSEL_4 | ADC12INCH_4; // Vr+ = VeREF+ (ext) and Vr-=AVss, 12bit resolution, channel 4
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
	ADC12MCTL6 = ADC12VRSEL_4 | ADC12INCH_10 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#else
			ADC12MCTL5 = ADC12VRSEL_4 | ADC12INCH_5 | ADC12EOS; // Vr+ = VeREF+ (ext) and Vr-=AVss,12bit resolution,channel 5,EOS
#endif
	ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;                   // sample a sequence
	ADC12CTL0 |= ADC12MSC; //first sample by trigger and rest automatic trigger by prior conversion
#endif

	//ADC interrupt logic
#if defined(MAX_NUM_SENSORS) && MAX_NUM_SENSORS == 5
	ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5 | ADC12IE6; // Enable ADC conv complete interrupt
#else
			ADC12IER0 |= ADC12IE2 | ADC12IE3 | ADC12IE4 | ADC12IE5; // Enable ADC conv complete interrupt
#endif

}

//--------------- TEMPERATURES ---------------------

float inline temperature_getValue(uint8_t id) {
	return g_pSysState->temp.sensors[id].fTemperature;
}

char *temperature_getString(uint8_t id) {
	// Check if the value is valid
	return g_pSysState->temp.sensors[id].temperature;
}

TEMPERATURE_SENSOR inline *sensor_get(uint8_t id) {
	return &g_pSysState->temp.sensors[id];
}

void temperature_sensor_init(uint8_t id) {
	TEMPERATURE_SENSOR *sensor = sensor_get(id);
	sensor->name[0] = SensorName[id][0];
	sensor->name[1] = 0;
}

void sensor_clear(uint8_t id) {
	TEMPERATURE_SENSOR *sensor;
	sensor = sensor_get(id);
	sensor->iADC = 0;
	sensor->fTemperature = 0;
	sensor->temperature[0] = 0;
}

void temperature_trigger_init() {
	USE_TEMPERATURE
	int c;

	tem->iCapturing = 0;
	tem->iSamplesRead = 0;

	//initialze ADCvar
	for (c = 0; c < MAX_NUM_SENSORS; c++) {
		sensor_get(c)->iADC = 0;
		sensor_get(c)->iSamplesRead = 0;
	}
}


float resistance_to_temperature(float R) {
	float A1 = 0.00335, B1 = 0.0002565, C1 = 0.0000026059, D1 = 0.00000006329,
			tempdegC;
	float R25 = 9965.0;

	tempdegC = 1
			/ (A1 + (B1 * log(R / R25)) + (C1 * pow((log(R / R25)), 2))
					+ (D1 * pow((log(R / R25)), 3)));

	tempdegC = tempdegC - 273.15;

	return tempdegC;
}

void temperature_subsampling_calculate(int8_t iSensorIdx) {
	float ADCval;
	float A0V2V, A0R2;
	float A0tempdegC = 0.0;

	char* TemperatureVal = temperature_getString(iSensorIdx);
	double *calibration;

	TEMPERATURE_SENSOR *sensor = sensor_get(iSensorIdx);
	if (sensor->iSamplesRead == 0)
		return;

	ADCval = (float) sensor->iADC / (float) sensor->iSamplesRead;
	A0V2V = 0.00061 * ADCval;		//Converting to voltage. 2.5/4096 = 0.00061

	A0R2 = (A0V2V * 10000.0) / (2.5 - A0V2V);			//R2= (V2*R1)/(V1-V2)

	//Convert resistance to temperature using Steinhart-Hart algorithm
	A0tempdegC = resistance_to_temperature(A0R2);

	//use calibration formula
	calibration = &g_pCalibrationCfg->calibration[iSensorIdx][0];

	if ((calibration[0] > -2.0) && (calibration[0] < 2.0)
			&& (calibration[1] > -2.0) && (calibration[1] < 2.0)) {
		A0tempdegC = A0tempdegC * calibration[1] + calibration[0];
	}

	sensor->fTemperature = A0tempdegC;
	getFloatNumber2Text(A0tempdegC, TemperatureVal);
}

void temperature_analog_to_digital_conversion() {
	USE_TEMPERATURE
	int iIdx;
	// Average sensors

	if (tem->iSamplesRequired == 0)
		return;

	// convert the current sensor ADC value to temperature
	for (iIdx = 0; iIdx < MAX_NUM_SENSORS; iIdx++)
		temperature_subsampling_calculate(iIdx);
}

void temperature_subsamples(uint8_t samples)  {
	USE_TEMPERATURE
	temperature_trigger_init();
	tem->iSamplesRequired = samples;
}

void temperature_trigger_capture() {
	USE_TEMPERATURE

	// SUBSAMPLING TOO CLOSE FOR THE TRIGGER TO WORK
	if (tem->iCapturing > 0 && tem->iCapturing <= MAX_NUM_SENSORS)
		return;

	// Turn on ADC conversion
	ADC12CTL0 &= ~ADC12ENC;
	ADC12CTL0 |= ADC12ENC | ADC12SC;

	if (tem->iCapturing == 0)
		tem->iCapturing = 1;
}

void temperature_single_capture() {
	USE_TEMPERATURE
	config_setLastCommand(COMMAND_TEMPERATURE_SAMPLE);
	if (tem->iSamplesRequired==0)
		return;

	if (tem->iSamplesRead == tem->iSamplesRequired) {
		tem->iSamplesRequired = 0;
		temperature_analog_to_digital_conversion();
		return;
	}
	temperature_trigger_capture();
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
	USE_TEMPERATURE
	switch (__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG)) {
	case ADC12IV_NONE:
		break;        // Vector  0:  No interrupt
	case ADC12IV_ADC12OVIFG:
		break;        // Vector  2:  ADC12MEMx Overflow
	case ADC12IV_ADC12TOVIFG:
		break;        // Vector  4:  Conversion time overflow
	case ADC12IV_ADC12HIIFG:
		break;        // Vector  6:  ADC12BHI
	case ADC12IV_ADC12LOIFG:
		break;        // Vector  8:  ADC12BLO
	case ADC12IV_ADC12INIFG:
		break;        // Vector 10:  ADC12BIN
	case ADC12IV_ADC12IFG0:                 // Vector 12:  ADC12MEM0 Interrupt
		if (ADC12MEM0 >= 0x7ff)               // ADC12MEM0 = A1 > 0.5AVcc?
			P1OUT |= BIT0;                      // P1.0 = 1
		else
			P1OUT &= ~BIT0;                     // P1.0 = 0
		__bic_SR_register_on_exit(LPM0_bits); // Exit active CPU
		break;                                // Clear CPUOFF bit from 0(SR)
	case ADC12IV_ADC12IFG1:
		break;        // Vector 14:  ADC12MEM1
	case ADC12IV_ADC12IFG2:   		        // Vector 16:  ADC12MEM2
		tem->sensors[0].iADC += ADC12MEM2;
		tem->sensors[0].iSamplesRead++;
		break;
	case ADC12IV_ADC12IFG3:   		        // Vector 18:  ADC12MEM3
		tem->sensors[1].iADC += ADC12MEM3;
		tem->sensors[1].iSamplesRead++;
		break;
	case ADC12IV_ADC12IFG4:   		        // Vector 20:  ADC12MEM4
		tem->sensors[2].iADC += ADC12MEM4;
		tem->sensors[2].iSamplesRead++;
		break;
	case ADC12IV_ADC12IFG5:   		        // Vector 22:  ADC12MEM5
		tem->sensors[3].iADC += ADC12MEM5;
		tem->sensors[3].iSamplesRead++;
		break;
	case ADC12IV_ADC12IFG6:                 // Vector 24:  ADC12MEM6
		tem->sensors[4].iADC += ADC12MEM6;
		tem->sensors[4].iSamplesRead++;
		break;
	case ADC12IV_ADC12RDYIFG:
		break;        // Vector 76:  ADC12RDY
	default:
		break;
	}

	tem->iCapturing++;
	if (tem->iCapturing == MAX_NUM_SENSORS + 1) {
		tem->iSamplesRead++;
		tem->iCapturing = 0;
		// Instantly run this event after the capture is finished
		if (tem->iSamplesRequired>1 && tem->iSamplesRead<=tem->iSamplesRequired)
			event_force_event_by_id(EVT_SUBSAMPLE_TEMP, 0);
		EVENT_WAKEUP
	}

}
