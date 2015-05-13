/*
 * lcd.h
 *
 *  Created on: May 13, 2015
 *      Author: sergioam
 */

#ifndef LCD_H_
#define LCD_H_

extern void lcd_init();
extern void lcd_show(int8_t iItemId);
extern void lcd_print(const uint8_t *pcData);
extern void lcd_print_line(const uint8_t* pcData, int8_t iLine);
extern void lcd_reset();
extern void lcd_blenable();
extern void lcd_bldisable();
extern void lcd_clear();
extern void lcd_on();
extern void lcd_off();
extern void lcd_setaddr(int8_t addr);

#endif /* LCD_H_ */
