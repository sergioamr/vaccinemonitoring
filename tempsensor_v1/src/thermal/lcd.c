#include "thermalcanyon.h"

char g_bLCD_state = 0;

void lcd_setupIO() {
	PJDIR |= BIT6 | BIT7;      			// set LCD reset and Backlight enable
	PJOUT |= BIT6;							// LCD reset pulled high
	PJOUT &= ~BIT7;							// Backlight disable
}

void lcd_init() {
	char lcdBuffer[16];

	lcd_turn_on();

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
	uint8_t lcdBuffer;
	lcdBuffer = 0x01;
	i2c_write(0x3e, 0, 1, (uint8_t *) &lcdBuffer);
}

void lcd_on() {
	uint8_t lcdBuffer;
	lcdBuffer = 0x0C;
	i2c_write(0x3e, 0, 1, (uint8_t *) &lcdBuffer);
	g_bLCD_state = 1;
}

void lcd_off() {
	uint8_t lcdBuffer = 0x08;
	i2c_write(0x3e, 0, 1, (uint8_t *) &lcdBuffer);
	g_bLCD_state = 0;
}

void lcd_setaddr(int8_t addr) {
	uint8_t lcdBuffer = addr | 0x80;
	i2c_write(0x3e, 0, 1, (uint8_t *) &lcdBuffer);
}

void lcd_turn_on() {
	lcd_blenable(); //enable backlight
	lcd_on(); 		//lcd reset
}

void lcd_turn_off() {
	lcd_bldisable();
	lcd_off();
}

void lcd_append_signal_info(char *lcdBuffer) {
	if (state_isSignalInRange()) {
		strcat(lcdBuffer, itoa_pad(state_getSignalPercentage()));
		strcat(lcdBuffer, "% ");
		if (state_isGPRS() == 1) {
			strcat(lcdBuffer, "G:YES");
		} else {
			strcat(lcdBuffer, "G:NO");
		}
	} else {
		strcat(lcdBuffer, " No signal ");
	}
}

void lcd_setDate(char *buffer) {
	rtc_getlocal(&g_tmCurrTime);
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_year));
	strcat(buffer, "/");
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_mon));
	strcat(buffer, "/");
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_mday));
	strcat(buffer, " ");

	strcat(buffer, itoa_pad(g_tmCurrTime.tm_hour));
	strcat(buffer, ":");
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_min));
}

void lcd_setUptime(char *buffer) {
	strcat(buffer, "[");
	strcat(buffer, itoa_pad(iMinuteTick/60));
	strcat(buffer, ":");
	strcat(buffer, itoa_pad(iMinuteTick%60));
	strcat(buffer, ":");
	strcat(buffer, itoa_pad(iSecondTick%60));
	strcat(buffer, "]");
	strcat(buffer, " ");
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_hour));
	strcat(buffer, ":");
	strcat(buffer, itoa_pad(g_tmCurrTime.tm_min));
}

void lcd_show() {

	static int8_t iItemId = -1;
	static time_t lastRefresh = 0;

	char lcdBuffer[LCD_DISPLAY_LEN + 1];
	int iIdx = 0;
	int iCnt = 0;

	// LCD is off
	if (g_bLCD_state == 0)
		return;

	if (iItemId == g_iDisplayId &&
		lastRefresh == rtc_get_second_tick())
		return;

	iItemId = g_iDisplayId;
	lastRefresh = rtc_get_second_tick();

	lcd_clear();

	memset(lcdBuffer, 0, sizeof(lcdBuffer));

#ifdef _DEBUG
	lcd_setUptime(lcdBuffer);
#else
	lcd_setDate(lcdBuffer);
#endif
	//get local time
	iIdx = strlen(lcdBuffer); //marker

	if (iItemId > 0 && iItemId <= 5) {
		iCnt = iItemId - 1;
	}

	switch (iItemId) {
	case 0:
		strcat(lcdBuffer, temperature_getString(0));
		strcat(lcdBuffer, "C ");
		strcat(lcdBuffer, itoa_pad(batt_getlevel()));
		strcat(lcdBuffer, "% ");
		if (state_isSignalInRange()) {
			if (modem_getNetworkService() == NETWORK_GPRS) {
				strcat(lcdBuffer, "G");
			} else {
				strcat(lcdBuffer, "S");
			}
			strcat(lcdBuffer, itoa_pad(state_getSignalPercentage()));
			strcat(lcdBuffer, "%");
		} else {
			strcat(lcdBuffer, "S --  ");
		}
		iCnt = 0xff;
		break;

	case 6:
		iCnt = 0xff;
		strcat(lcdBuffer, itoa_pad(batt_getlevel()));
		strcat(lcdBuffer, "% ");
		if (state_getAlarms()->alarms.battery) {
			strcat(lcdBuffer, "BATT ALERT");
		} else
		//power not plugged
		if (!POWER_ON) {
			strcat(lcdBuffer, "POWER OUT");
		// (P4IN & BIT6) = LED2 ON
		} else if (((P4IN & BIT6)) && (batt_getlevel() == 100)) {
			strcat(lcdBuffer, "FULL CHARGE");
		} else {
			strcat(lcdBuffer, "CHARGING");
		}

		break;
		// added for new display//
	case 7:
		iCnt = 0xff;
		strcat(lcdBuffer, "SIM1 ");	//current sim slot is 1
		if (config_getSelectedSIM() != 0)
			strcat(lcdBuffer, "  --  ");
		else
			lcd_append_signal_info(lcdBuffer);
		break;
	case 8:
		iCnt = 0xff;
		strcat(lcdBuffer, "SIM2 ");	//current sim slot is 2
		if (config_getSelectedSIM() != 1)
			strcat(lcdBuffer, "  --  ");
		else
			lcd_append_signal_info(lcdBuffer);
		break;

#ifdef _DEBUG
	case 9+5:
		lcd_display_config();
		return;
#endif

	default:
		break;
	}

#ifdef _DEBUG
	if (iItemId>=9 && iItemId<9+5) {
		lcd_display_config_sensor(iItemId-9);
		return;
	} else
#endif
	if (iCnt != 0xff) {
		if (g_pSysState->temp.state[iCnt].status != 0) {
			sprintf(&lcdBuffer[iIdx], "ALERT %s %sC", SensorName[iCnt],
					temperature_getString(iCnt));
		} else {
			sprintf(&lcdBuffer[iIdx], "Sensor %s %sC", SensorName[iCnt],
					temperature_getString(iCnt));
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
	if (count <= 0) {
		lcd_print_progress();
		return;
	}

#ifdef _DEBUG
	checkStack();
#endif
	for (t = 0; t < count; t++) {
		delay(50);
		lcd_print_progress();
	}
}

void lcd_printf(int line, const char *_format, ...) {
	va_list _ap;
	char szTemp[33];

#ifdef _DEBUG
	checkStack();
#endif

	va_start(_ap, _format);
	vsnprintf(szTemp, sizeof(szTemp), _format, _ap);
	va_end(_ap);

	lcd_printl(line, szTemp);
}

void lcd_print(char* pcData) {
	size_t len = strlen(pcData);

	if (g_bLCD_state == 0)
		return;

	if (len > LCD_LINE_LEN) {
		len = LCD_LINE_LEN;
	}
	lcd_clear();
	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);
}

void lcd_printl(int8_t iLine, const char* pcData) {
	size_t len = strlen(pcData);

	if (g_bLCD_state == 0)
		return;

	if (iLine == LINEC)
		lcd_clear();

	if (len > LCD_LINE_LEN)
		len = LCD_LINE_LEN;

	if (iLine == LINE2 || iLine == LINEH || iLine == LINEE)
		lcd_setaddr(0x40);
	else
		lcd_setaddr(0x0);

	i2c_write(0x3e, 0x40, len, (uint8_t *) pcData);

	if (iLine == LINEE) {
		delay(HUMAN_DISPLAY_ERROR_DELAY);
	}

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

const char display[4] = { '*', '|', '/', '-' };
void lcd_print_progress() {
	if (g_bLCD_state == 0)
		return;

	static char pos = 0;
	lcd_setaddr(0x4F);
	i2c_write(0x3e, 0x40, 1, (uint8_t *) &display[(++pos) & 0x3]);
}

void lcd_print_boot(const char* pcData, int line) {
	if (g_iLCDVerbose == VERBOSE_DISABLED)
		return;

#ifdef _DEBUG
	if (g_bLCD_state == 0)
	return;

	if (line==1)
	lcd_clear();

	lcd_printl(line, pcData);
#else
	lcd_print_progress();
#endif
}

void lcd_display_config_sensor(int id) {
	TEMP_ALERT_PARAM *pAlertParams = &g_pDevCfg->stTempAlertParams[id];
	lcd_printf(LINEC, "%s Cold %dm %d", SensorName[id], (int) pAlertParams->maxSecondsCold / 60, (int) pAlertParams->threshCold);
	lcd_printf(LINE2, "Hot %dm %d", (int) pAlertParams->maxSecondsHot / 60, (int) pAlertParams->threshHot);
}

void lcd_display_config() {
	TEMP_ALERT_PARAM *pAlertParams = &g_pDevCfg->stTempAlertParams[0];
	char num1[TEMP_DATA_LEN + 1];
	char num2[TEMP_DATA_LEN + 1];

	getFloatNumber2Text(pAlertParams->threshCold, num1);
	getFloatNumber2Text(pAlertParams->threshHot, num2);

#ifdef _DEBUG
	checkStack();
	lcd_printf(LINEC, "%x %d %s %d %s", g_pSysCfg->stackLeft, (int) pAlertParams->maxSecondsCold / 60,
			&num1[0], (int) pAlertParams->maxSecondsHot / 60, &num2[0]);
#else
	lcd_printf(LINEC, "C%d %s H%d %s", (int) pAlertParams->maxSecondsCold / 60,
			&num1[0], (int) pAlertParams->maxSecondsHot / 60, &num2[0]);
#endif
	lcd_printf(LINE2, "S%d U%d L%d P%d", g_pDevCfg->cfgSelectedSIM_slot + 1,
			g_pDevCfg->sIntervalsMins.upload,
			g_pDevCfg->sIntervalsMins.sampling,
			g_pDevCfg->stBattPowerAlertParam.minutesPower);
}
