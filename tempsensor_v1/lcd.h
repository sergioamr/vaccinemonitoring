/*
 * lcd.h
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 */

#ifndef TEMPSENSOR_V1_LCD_H_
#define TEMPSENSOR_V1_LCD_H_

//LCD lines
#define LINE1					1
#define LINE2					2
// Line 1 and clear screen
#define LINEC					0

// Line 2 wait for user to read
#define LINEH					3

// Line 2 wait for user to read ERROR
#define LINEE					4

#define VERBOSE_BOOTING 0
#define VERBOSE_DISABLED -1

// Time to display an error to a human
#ifdef _DEBUG
#define HUMAN_DISPLAY_ERROR_DELAY 3000
#define HUMAN_DISPLAY_INFO_DELAY 1000
#define HUMAN_DISPLAY_LONG_INFO_DELAY 2000
#else
#define HUMAN_DISPLAY_ERROR_DELAY 10000
#define HUMAN_DISPLAY_INFO_DELAY 3000
#define HUMAN_DISPLAY_LONG_INFO_DELAY 6000
#endif

extern int g_iLCDVerbose;

// Encapsulation, this should go into a class
void lcd_setVerboseMode(int v);
int lcd_getVerboseMode();
void lcd_disable_verbose();
void lcd_enable_verbose();

void lcd_setupIO();
void lcd_reset();
void lcd_blenable();
void lcd_init();
void lcd_on();
void lcd_off();
void lcd_clear();
void lcd_setaddr(int8_t addr);
void lcd_show();
void lcd_turn_on();
void lcd_turn_off();
void lcd_print(char* pcData);
int lcd_printf(int line, const char *_format, ...);
void lcd_printl(int8_t iLine, const char* pcData);
void lcd_print_boot(const char* pcData, int line);
void lcd_bldisable();
void lcd_progress_wait(uint16_t delayTime);
void lcd_print_progress();

#endif /* TEMPSENSOR_V1_LCD_H_ */
