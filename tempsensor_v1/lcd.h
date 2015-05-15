/*
 * lcd.h
 *
 *  Created on: May 15, 2015
 *      Author: sergioam
 */

#ifndef LCD_H_
#define LCD_H_

//LCD lines
#define LINE1					1
#define LINE2					2

extern void lcd_init();
extern void lcd_clear();
extern void lcd_setaddr(int8_t addr);
extern void lcd_show(int8_t iItemId);
extern void lcd_print(char* pcData);
extern void lcd_print_line(char* pcData,int8_t iLine);
extern void lcd_print_debug(char* pcData);

#endif /* LCD_H_ */
