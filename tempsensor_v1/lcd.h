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

#define VERBOSE_BOOTING 0
#define VERBOSE_DISABLED -1

// Time to display an error to a human
#define HUMAN_DISPLAY_ERROR_DELAY 10000
#define HUMAN_DISPLAY_INFO_DELAY 1000

extern int g_iLCDVerbose;

extern void lcd_disable_verbose();
extern void lcd_enable_verbose();
extern void lcd_setupIO();
extern void lcd_reset();
extern void lcd_blenable();
extern void lcd_init();
extern void lcd_on();
extern void lcd_off();
extern void lcd_clear();
extern void lcd_setaddr(int8_t addr);
extern void lcd_show(int8_t iItemId);
extern void lcd_print(char* pcData);
extern int lcd_print_ext(int line, const char *_format, ...);
extern void lcd_print_lne(int8_t iLine, const char* pcData);
extern void lcd_print_progress(const char* pcData, int line);
extern void lcd_bldisable();
extern void lcd_progress_wait(uint16_t delayTime);
#endif /* TEMPSENSOR_V1_LCD_H_ */
