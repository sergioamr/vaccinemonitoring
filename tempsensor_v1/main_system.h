/*
 * main_system.h
 *
 *  Created on: Jun 2, 2015
 *      Author: sergioam
 */

#ifndef MAIN_SYSTEM_H_
#define MAIN_SYSTEM_H_

extern int g_iRunning;
char system_isRunning();
void system_reboot(const char *message);

#endif /* MAIN_SYSTEM_H_ */
