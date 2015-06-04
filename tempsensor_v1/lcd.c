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
#include "temperature.h"

void lcd_setupIO() {
	PJDIR |= BIT6 | BIT7;      			// set LCD reset and Backlight enable
	PJOUT |= BIT6;							// LCD reset pulled high
	PJOUT &= ~BIT7;							// Backlight disable
}

char lcdBuffer[32];

void lcd_init() {

	config_setLastCommand(COMMAND_LCDINIT);
	memset(lcdBuffer, 0, LCD_INIT_PARAM_SIZE);

	lcdBuffer[0] = 0x38; // Basic
	lcdBuffer[1] = 0x39; // Extended
	lcdBuffer[2] = 0x14; // OCS frequency adjustment
	lcdBuffer[3] = 0x78;
	lcdBuffer[4] = 0x5E;
	lcdBuffer[5] = 0x6D;
	lcdBuffer[6] = 0x0C; // display on
	lcdBuffer[7] = 0x01; // clear display
	lcdBuffer[8] = 0x06; // entry mode set

	i2c_write(0x3e, 0, LCD_INIT_PARAM_SIZE, (uint8_t*) lcdBuffer);

	lcdBuffer[0] = 0x40 | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t *) lcdBuffer);

#if TESTING_LCD
	// DISPLAY AB CD just for testing
	lcdBuffer[0] = 0x43;
	lcdBuffer[1] = 0x44;
	i2c_write(0x3e, 0x40, 2, lcdBuffer);

	lcdBuffer[0] = 0x41;
	lcdBuffer[1] = 0x42;
	i2c_write(0x3e,0x40,2,lcdBuffer);
#endif
	delay(1000);
}

void lcd_clear() {
	lcdBuffer[0] = 0x01;
	i2c_write(0x3e, 0, 1, (uint8_t *) lcdBuffer);
}

void lcd_on() {
	lcdBuffer[0] = 0x0C;
	i2c_write(0x3e, 0, 1, (uint8_t *) lcdBuffer);
}

void lcd_off() {
	lcdBuffer[0] = 0x08;
	i2c_write(0x3e, 0, 1, (uint8_t *) lcdBuffer);
}

void lcd_setaddr(int8_t addr) {
	lcdBuffer[0] = addr | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t *) lcdBuffer);
}

void lcd_show(int8_t iItemId) {
	int iIdx = 0;
	int iCnt = 0;
	//float signal_strength = 0;
	float local_signal = 0;

	//check if there is a change in display id
	//if(iLastDisplayId != iItemId) lcd_clear();
	lcd_clear();

	memset(lcdBuffer, 0, LCD_DISPLAY_LEN);

	//get local time
	rtc_getlocal(&g_tmCurrTime);
	strcat(lcdBuffer, itoa_pad(g_tmCurrTime.tm_year));
	strcat(lcdBuffer, "/");
	strcat(lcdBuffer, itoa_pad(g_tmCurrTime.tm_mon));
	strcat(lcdBuffer, "/");
	strcat(lcdBuffer, itoa_pad(g_tmCurrTime.tm_mday));
	strcat(lcdBuffer, " ");

	strcat(lcdBuffer, itoa_pad(g_tmCurrTime.tm_hour));
	strcat(lcdBuffer, ":");
	strcat(lcdBuffer, itoa_pad(g_tmCurrTime.tm_min));
	iIdx = strlen(lcdBuffer); //marker

	switch (iItemId) {
	case 0:
		memset(&Temperature[1][0], 0, TEMP_DATA_LEN + 1); //initialize as it will be used as scratchpad during POST formatting
		digital_amp_to_temp_string(ADCvar[1], &Temperature[1][0], 1);
		strcat(lcdBuffer, Temperature[1]);
		strcat(lcdBuffer, "C ");
		strcat(lcdBuffer, itoa_pad(g_iBatteryLevel));
		strcat(lcdBuffer, "% ");
		if ((iSignalLevel >= NETWORK_DOWN_SS)
				&& (iSignalLevel <= NETWORK_MAX_SS)) {
			if (g_iSignal_gprs == 1) {
				strcat(lcdBuffer, "G");
			} else {
				strcat(lcdBuffer, "S");
			}
			local_signal = iSignalLevel;
			local_signal = (((local_signal - NETWORK_ZERO)
					/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
			strcat(lcdBuffer, itoa_pad(local_signal));
			strcat(lcdBuffer, "%");
		} else {
			strcat(lcdBuffer, "S --  ");
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

		strcat(lcdBuffer, itoa_pad(g_iBatteryLevel));
		strcat(lcdBuffer, "% ");
		if (TEMP_ALARM_GET(MAX_NUM_SENSORS) == TEMP_ALERT_CNF) {
			strcat(lcdBuffer, "BATT ALERT");
		} else if (P4IN & BIT4)	//power not plugged
		{
			strcat(lcdBuffer, "POWER OUT");
		} else if (((P4IN & BIT6)) && (g_iBatteryLevel == 100)) {
			strcat(lcdBuffer, "FULL CHARGE");
		} else {
			strcat(lcdBuffer, "CHARGING");
		}

		break;
		// added for new display//
	case 7:
		iCnt = 0xff;
		strcat(lcdBuffer, "SIM1 ");	//current sim slot is 1
		if (config_getSelectedSIM() != 0) {
			strcat(lcdBuffer, "  --  ");
		} else {
			if ((iSignalLevel > NETWORK_DOWN_SS)
					&& (iSignalLevel < NETWORK_MAX_SS)) {
				local_signal = iSignalLevel;
				local_signal = (((local_signal - NETWORK_ZERO)
						/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
				strcat(lcdBuffer, itoa_pad(local_signal));
				strcat(lcdBuffer, "% ");
				if (g_iSignal_gprs == 1) {
					strcat(lcdBuffer, "G:YES");
				} else {
					strcat(lcdBuffer, "G:NO");
				}
			} else {
				strcat(lcdBuffer, "  --  ");
			}
		}
		break;
	case 8:
		iCnt = 0xff;
		strcat(lcdBuffer, "SIM2 ");	//current sim slot is 2
		if (config_getSelectedSIM() != 1) {
			strcat(lcdBuffer, "  --  ");
		} else {
			if ((iSignalLevel > NETWORK_DOWN_SS)
					&& (iSignalLevel < NETWORK_MAX_SS)) {
				local_signal = iSignalLevel;
				local_signal = (((local_signal - NETWORK_ZERO)
						/ (NETWORK_MAX_SS - NETWORK_ZERO)) * 100);
				strcat(lcdBuffer, itoa_pad(local_signal));
				strcat(lcdBuffer, "% ");
				if (g_iSignal_gprs == 1) {
					strcat(lcdBuffer, "G:YES");
				} else {
					strcat(lcdBuffer, "G:NO");
				}
			} else {
				strcat(lcdBuffer, "  --  ");
			}
		}
		break;
	default:
		break;
	}

	if (iCnt != 0xff) {
		memset(&Temperature[iCnt], 0, TEMP_DATA_LEN + 1);//initialize as it will be used as scratchpad during POST formatting
		digital_amp_to_temp_string(ADCvar[iCnt], &Temperature[iCnt][0], iCnt);

		if (TEMP_ALARM_GET(iCnt) == TEMP_ALERT_CNF) {
			strcat(lcdBuffer, "ALERT ");
			strcat(lcdBuffer, SensorName[iCnt]);
			strcat(lcdBuffer, " ");
			strcat(lcdBuffer, Temperature[iCnt]);
			strcat(lcdBuffer, "C ");
		} else {
			strcat(lcdBuffer, "Sensor ");
			strcat(lcdBuffer, SensorName[iCnt]);
			strcat(lcdBuffer, " ");
			strcat(lcdBuffer, Temperature[iCnt]);
			strcat(lcdBuffer, "C ");
		}
	}

	//display the lines
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, (uint8_t *) lcdBuffer);
	lcd_setaddr(0x40);	//go to next line
	i2c_write(0x3e, 0x40, LCD_LINE_LEN, (uint8_t *) &lcdBuffer[iIdx]);
}

void lcd_progress_wait(uint16_t delayTime) {
	int t;
	int count = delayTime / 100;

	for (t = 0; t < count; t++) {
		delay(50);
		lcd_print_boot("", LINE2);
	}
}

int lcd_printf(int line, const char *_format, ...) {
	char szTemp[32];
	va_list _ap;
	int rval;
	char *fptr = (char *) _format;
	char *out_end = szTemp;

	va_start(_ap, _format);
	rval = __TI_printfi(&fptr, _ap, (void *) &out_end, _outc, _outs);
	va_end(_ap);

	*out_end = '\0';
	lcd_printl(line, szTemp);
	return (rval);
}

void lcd_print(char* pcData) {
	size_t len = strlen(pcData);

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	lcd_clear();
	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);
}

void lcd_printl(int8_t iLine, const char* pcData) {
	size_t len = strlen(pcData);

	if (iLine == LINEC)
		lcd_clear();

	if (len > LCD_LINE_LEN)
		len = LCD_LINE_LEN;

	if (iLine == LINE2 || iLine == LINEH)
		lcd_setaddr(0x40);
	else
		lcd_setaddr(0x0);

	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);

	if (iLine == LINEE)
		delay(HUMAN_DISPLAY_ERROR_DELAY);

	if (iLine == LINEH)
		delay(HUMAN_DISPLAY_INFO_DELAY);
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

int g_iLCDVerbose = VERBOSE_DISABLED; // Disable debug
void lcd_disable_verbose() {
	g_iLCDVerbose = VERBOSE_DISABLED;
}

void lcd_setVerboseMode(int value) {
	g_iLCDVerbose = value;
}

int lcd_getVerboseMode() {
	return g_iLCDVerbose;
}

void lcd_enable_verbose() {
	g_iLCDVerbose = VERBOSE_BOOTING;
}

void lcd_print_progress() {
	static char pos = 0;
	char display[4] = { '*', '|', '/', '-' };
	lcd_setaddr(0x0F);
	i2c_write(0x3e, 0x40, 1, (uint8_t *) &display[(++pos) & 0x3]);
}

void lcd_print_boot(const char* pcData, int line) {
	if (g_iLCDVerbose == VERBOSE_DISABLED)
		return;

#ifdef _DEBUG
	lcd_clear();
	lcd_printl(line, pcData);
#else
	lcd_print_progress();
#endif
}
