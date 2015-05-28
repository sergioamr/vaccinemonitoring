#include "stdint.h"
#include "i2c.h"
#include "config.h"
#include "lcd.h"
#include "time.h"
#include "stdio.h"
#include "string.h"
#include "timer.h"
#include "globals.h"
#include "rtc.h"
#include "stringutils.h"

void lcd_setupIO() {
	PJDIR |= BIT6 | BIT7;      			// set LCD reset and Backlight enable
	PJOUT |= BIT6;							// LCD reset pulled high
	PJOUT &= ~BIT7;							// Backlight disable
}

void lcd_init() {
	memset(SampleData, 0, LCD_INIT_PARAM_SIZE);

	SampleData[0] = 0x38; // Basic
	SampleData[1] = 0x39; // Extended
	SampleData[2] = 0x14; // OCS frequency adjustment
	SampleData[3] = 0x78;
	SampleData[4] = 0x5E;
	SampleData[5] = 0x6D;
	SampleData[6] = 0x0C; // display on
	SampleData[7] = 0x01; // clear display
	SampleData[8] = 0x06; // entry mode set

	i2c_write(0x3e, 0, LCD_INIT_PARAM_SIZE,  (uint8_t*) SampleData);
	delay(100);

	SampleData[0] = 0x40 | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t *) SampleData);

	//SampleData[0] = 0x43;
	//SampleData[1] = 0x44;
	//i2c_write(0x3e, 0x40, 2, SampleData);
	//delay(100);
#if 0
	delay(1000);

	SampleData[0] = 0x41;
	SampleData[1] = 0x42;
	i2c_write(0x3e,0x40,2,SampleData);
	delay(100);
#endif

	delay(1000);
}

void lcd_clear() {
	SampleData[0] = 0x01;
	i2c_write(0x3e, 0, 1, (uint8_t *) SampleData);
	delay(100);
}

void lcd_on() {
	SampleData[0] = 0x0C;
	i2c_write(0x3e, 0, 1, (uint8_t *) SampleData);
	delay(100);
}

void lcd_off() {
	SampleData[0] = 0x08;
	i2c_write(0x3e, 0, 1, (uint8_t *) SampleData);
	delay(100);
}

void lcd_setaddr(int8_t addr) {
	SampleData[0] = addr | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t *) SampleData);
	delay(100);
}

void lcd_show(int8_t iItemId) {
	int iIdx = 0;
	int iCnt = 0;
	//float signal_strength = 0;
	float local_signal = 0;

	//check if there is a change in display id
	//if(iLastDisplayId != iItemId) lcd_clear();
	lcd_clear();

	memset(SampleData, 0, LCD_DISPLAY_LEN);
	//get local time
	rtc_getlocal(&currTime);
	strcat(SampleData, itoa(currTime.tm_year));
	strcat(SampleData, "/");
	strcat(SampleData, itoa(currTime.tm_mon));
	strcat(SampleData, "/");
	strcat(SampleData, itoa(currTime.tm_mday));
	strcat(SampleData, " ");

	strcat(SampleData, itoa(currTime.tm_hour));
	strcat(SampleData, ":");
	strcat(SampleData, itoa(currTime.tm_min));
	iIdx = strlen(SampleData); //marker

	switch (iItemId) {
	case 0:
		memset(&Temperature[1][0], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
		ConvertADCToTemperature(ADCvar[1], &Temperature[1][0], 1);
		strcat(SampleData, Temperature[1]);
		strcat(SampleData, "C ");
		strcat(SampleData, itoa(iBatteryLevel));
		strcat(SampleData, "% ");
		//  if(signal_gprs==1){
		if ((iSignalLevel >= NETWORK_DOWN_SS)
				&& (iSignalLevel <= NETWORK_MAX_SS)) {
			//	  strcat(SampleData,"G:");
			if (signal_gprs == 1) {
				strcat(SampleData, "G");
			} else {
				strcat(SampleData, "S");
			}
			local_signal = iSignalLevel;
			local_signal = (((local_signal - NETWORK_ZERO)
					/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
			strcat(SampleData, itoa(local_signal));
			strcat(SampleData, "%");
		} else {
			strcat(SampleData, "S --  ");
		}
		iCnt = 0xff;
		break;

	case 1:
		iCnt = 0;
		break;
	case 2:
		iCnt = 1;
		break;
	case 3:
		iCnt = 2;
		break;
	case 4:
		iCnt = 3;
		break;
#if defined(MAX_NUM_SENSORS) & MAX_NUM_SENSORS == 5
	case 5:
		iCnt = 4;
		break;
	case 6:
		iCnt = 0xff;
#else
		case 5: iCnt = 0xff;
#endif

		strcat(SampleData, itoa(iBatteryLevel));
		strcat(SampleData, "% ");
		if (TEMP_ALARM_GET(MAX_NUM_SENSORS) == TEMP_ALERT_CNF) {
			strcat(SampleData, "BATT ALERT");
		} else if (P4IN & BIT4)	//power not plugged
		{
			strcat(SampleData, "POWER OUT");
		} else if (((P4IN & BIT6)) && (iBatteryLevel == 100)) {
			strcat(SampleData, "FULL CHARGE");
		} else {
			strcat(SampleData, "CHARGING");
		}

		break;
		// added for new display//
	case 7:
		iCnt = 0xff;
		strcat(SampleData, "SIM1 ");	//current sim slot is 1
		if (g_pInfoA->cfgSIMSlot != 0) {
			strcat(SampleData, "  --  ");
		} else {
			if ((iSignalLevel > NETWORK_DOWN_SS)
					&& (iSignalLevel < NETWORK_MAX_SS)) {
				local_signal = iSignalLevel;
				local_signal = (((local_signal - NETWORK_ZERO)
						/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
				strcat(SampleData, itoa(local_signal));
				strcat(SampleData, "% ");
				if (signal_gprs == 1) {
					strcat(SampleData, "G:YES");
				} else {
					strcat(SampleData, "G:NO");
				}
			} else {
				strcat(SampleData, "  --  ");
			}
		}
		break;
	case 8:
		iCnt = 0xff;
		strcat(SampleData, "SIM2 ");	//current sim slot is 2
		if (g_pInfoA->cfgSIMSlot != 1) {
			strcat(SampleData, "  --  ");
		} else {
			if ((iSignalLevel > NETWORK_DOWN_SS)
					&& (iSignalLevel < NETWORK_MAX_SS)) {
				local_signal = iSignalLevel;
				local_signal = (((local_signal - NETWORK_ZERO)
						/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
				strcat(SampleData, itoa(local_signal));
				strcat(SampleData, "% ");
				if (signal_gprs == 1) {
					strcat(SampleData, "G:YES");
				} else {
					strcat(SampleData, "G:NO");
				}
			} else {
				strcat(SampleData, "  --  ");
			}
		}
		break;
	default:
		break;
	}

	if (iCnt != 0xff) {
		memset(&Temperature[iCnt], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
		ConvertADCToTemperature(ADCvar[iCnt], &Temperature[iCnt][0], iCnt);

		if (TEMP_ALARM_GET(iCnt) == TEMP_ALERT_CNF) {
			strcat(SampleData, "ALERT ");
			strcat(SampleData, SensorName[iCnt]);
			strcat(SampleData, " ");
			strcat(SampleData, Temperature[iCnt]);
			strcat(SampleData, "C ");
		} else {
			strcat(SampleData, "Sensor ");
			strcat(SampleData, SensorName[iCnt]);
			strcat(SampleData, " ");
			strcat(SampleData, Temperature[iCnt]);
			strcat(SampleData, "C ");
		}
	}

	//display the lines
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, (uint8_t *) SampleData);
	delay(100);
	lcd_setaddr(0x40);	//go to next line
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, (uint8_t *) &SampleData[iIdx]);
}

void lcd_print(char* pcData) {
	int8_t len = strlen(pcData);

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	lcd_clear();
	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);
	delay(100); // Give time for the buffer to be copied through the i2c
}

void lcd_print_line(const char* pcData, int8_t iLine) {
	int8_t len = strlen(pcData);

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	if (iLine == 2) {
		lcd_setaddr(0x40);
	} else {
		lcd_setaddr(0x0);
	}
	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);
	delay(100); // Give time for the buffer to be copied through the i2c
}

void lcd_reset() {
	PJOUT &= ~BIT6;
	delay(100);	//delay 100 ms
	PJOUT |= BIT6;
}

void lcd_blenable() {
	PJOUT |= BIT7;
}

void lcd_bldisable() {
	PJOUT &= ~BIT7;
}

int g_iBooting = -1; // Disable debug
void lcd_disable_debug() {
	g_iBooting=-1;
}

void lcd_print_debug(const char* pcData, int line) {
#ifndef _DEBUG
	static char pos = 0;
	char display[4] = { '*', '|', '/', '-' };
#endif

	if (g_iBooting == -1)
		return;

#ifdef _DEBUG
	lcd_print_line(pcData, line);
#else
	lcd_setaddr(0x0D);
	i2c_write(0x3e,0x40,1,(uint8_t *)&display[(++pos)&0x3]);
#endif
	delay(50);
}
