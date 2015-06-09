/*
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "main_system.h"

#define SECONDS_(s) (s)
#define MINUTES_(m) (m*60L)
#define HOURS_(h)   (h*60L*60L)

EVENT_MANAGER g_sEvents;

/*******************************************************************************************************/
/* Event functions */
/*******************************************************************************************************/

void event_pull_time(time_t currentTime) {
	config_setLastCommand(COMMAND_BOOT_MIDNIGHT);
	modem_pull_time();

	// We update all the timers according to the new time
	events_sync(thermal_update_time());
}

void event_change_display(time_t currentTime) {
	config_setLastCommand(COMMAND_MAIN_LCD_DISPLAY);
	config_update_system_time();
	lcd_show(g_iDisplayId);
}

/*******************************************************************************************************/
/* Event based system */
/*******************************************************************************************************/

// Populates the next event index depending on event times
void events_find_next_event(time_t currentTime) {
	int t;
	EVENT *pEvent;
	time_t nextEventTime = UINT32_MAX;
	uint8_t nextEvent = 0;

	if (g_sEvents.registeredEvents == 0)
		return;

	nextEvent = 0;

	// Search for the event that is closer in time
	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		pEvent = &g_sEvents.events[t];
		if (pEvent->nextEventRun > currentTime
				&& pEvent->nextEventRun < nextEventTime) {
			nextEventTime = pEvent->nextEventRun;
			nextEvent = t;
		}
	}

	g_sEvents.nextEvent = nextEvent;
}

void events_register(char *name, time_t startTime, time_t interval,
		void (*functionCall)(time_t)) {
	EVENT *pEvent;
	uint8_t nextEvent;

	// Not enough space to register an event.
	if (g_sEvents.registeredEvents >= MAX_EVENTS)
		return;

	nextEvent = g_sEvents.registeredEvents;

	pEvent = &g_sEvents.events[nextEvent];
	strncpy(pEvent->name, name, sizeof(pEvent->name));
	pEvent->interval = interval;
	pEvent->lastEventRun = 0;
	pEvent->nextEventRun = 0;
	pEvent->startTime = startTime;
	g_sEvents.registeredEvents++;
}

void event_init(EVENT *pEvent, time_t currentTime) {
	if (pEvent->nextEventRun != 0)
		return;

	pEvent->nextEventRun = currentTime+pEvent->interval+pEvent->startTime;
}

void events_sync(time_t currentTime) {
	EVENT *pEvent;
	int t;

	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		pEvent = &g_sEvents.events[t];
		event_init(pEvent, currentTime);
	}
	events_find_next_event(0);
}

void events_run(time_t currentTime) {
	EVENT *pEvent;
	uint8_t nextEvent = g_sEvents.nextEvent;

	if (nextEvent > MAX_EVENTS)
		return;

	pEvent = &g_sEvents.events[nextEvent];
	if (pEvent == NULL)
		return;

	if (currentTime <= pEvent->nextEventRun)
		return;

	config_save_command(pEvent->name);
	events_find_next_event(currentTime);
	pEvent->callback(currentTime);
}

void events_init() {
	memset(&g_sEvents, 0, sizeof(g_sEvents));
	events_register("PULL TIME",    0, HOURS_(12), &event_pull_time);
	events_register("DISPLAY", 0, MINUTES_(LCD_REFRESH_INTERVAL), &event_change_display);
}