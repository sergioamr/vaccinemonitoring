/*
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "main_system.h"
#include "hardware_buttons.h"
#include "sms.h"

#define SECONDS_(s) (s)
#define MINUTES_(m) (m*60L)
#define HOURS_(h)   (h*60L*60L)

EVENT_MANAGER g_sEvents;

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
		if (pEvent->nextEventRun < nextEventTime) {
			nextEventTime = pEvent->nextEventRun;
			nextEvent = t;
		}
	}

	g_sEvents.nextEvent = nextEvent;
}

void events_register(EVENT_IDS id, char *name, time_t startTime, time_t interval,
		void (*functionCall)(void *, time_t)) {
	EVENT *pEvent;
	uint8_t nextEvent;

	// Not enough space to register an event.
	if (g_sEvents.registeredEvents >= MAX_EVENTS)
		return;

	nextEvent = g_sEvents.registeredEvents;

	pEvent = &g_sEvents.events[nextEvent];
	pEvent->id = id;
	strncpy(pEvent->name, name, sizeof(pEvent->name));
	pEvent->interval = interval;
	pEvent->lastEventRun = 0;
	pEvent->nextEventRun = 0;
	pEvent->startTime = startTime;
	pEvent->callback = functionCall;
	g_sEvents.registeredEvents++;
}

void event_init(EVENT *pEvent, time_t currentTime) {
	if (pEvent->nextEventRun != 0)
		return;

	pEvent->nextEventRun = currentTime + pEvent->interval + pEvent->startTime;
}

void event_next(EVENT *pEvent, time_t currentTime) {
	pEvent->lastEventRun = pEvent->nextEventRun;
	pEvent->nextEventRun += pEvent->interval;
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

void events_debug(time_t currentTime) {
#ifdef _DEBUG
	EVENT *pEvent = &g_sEvents.events[g_sEvents.nextEvent];
	time_t nextEventTime = pEvent->nextEventRun - currentTime;
	int test = nextEventTime%10;
	if (test==0 || nextEventTime<10)
		lcd_printf(LINE2, "[%s] %d  ", pEvent->name, nextEventTime);
#endif
}

void events_run(time_t currentTime) {
	EVENT *pEvent;
	uint8_t nextEvent = g_sEvents.nextEvent;

	if (nextEvent > MAX_EVENTS)
		return;

	pEvent = &g_sEvents.events[nextEvent];
	if (pEvent == NULL)
		return;

	if (currentTime < pEvent->nextEventRun)
		return;

	// Disable hardware interruptions before running the actions
	hardware_disable_buttons();

	config_save_command(pEvent->name);
	// Move the interval to the next time
	event_next(pEvent, currentTime);
	pEvent->callback(pEvent, currentTime);
	events_find_next_event(currentTime);

	// Invalidate display
	lcd_show();
	hardware_enable_buttons();
}

/*******************************************************************************************************/
/* Event functions */
/*******************************************************************************************************/

void event_sms_test(void *event, time_t currentTime) {
	sms_send_data_request(LOCAL_TESTING_NUMBER);
}

void event_SIM_check_incoming_msgs(void *event, time_t currentTime) {

	//TODO process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place

	EVENT *pEvent = (EVENT *) event;
	sms_process_messages();
	if (g_pDevCfg->cfgServerConfigReceived == 1) {
		pEvent->interval=MINUTES_(MSG_REFRESH_INTERVAL);
		pEvent->nextEventRun = pEvent->nextEventRun + pEvent->interval;
	}
}

void event_pull_time(void *event, time_t currentTime) {
	modem_pull_time();
	// We update all the timers according to the new time
	events_sync(thermal_update_time());
}

void event_update_display(void *event, time_t currentTime) {
	config_update_system_time();
}

// modem_check_network(); // Checks network and if it is down it does the swapping
void event_sample_temperature(void *event, time_t currentTime) {
	temperature_sample();
}

void events_init() {
	memset(&g_sEvents, 0, sizeof(g_sEvents));
#ifdef _DEBUG
	events_register(EVT_SMS_TEST, "SMS_TEST", 0,  MINUTES_(1), &event_sms_test);
	events_register(EVT_PULLTIME, "PULLTIME", 0,  HOURS_(1), &event_pull_time);
	events_register(EVT_DISPLAY, "DISPLAY", 0, SECONDS_(60), &event_update_display);

	// Check every 30 seconds until we get the configuration message;
	events_register(EVT_SMSCHECK, "SMSCHECK", 0, SECONDS_(30),
			&event_SIM_check_incoming_msgs);

#else
	events_register(EVT_PULLTIME, "PULLTIME", 0, HOURS_(12), &event_pull_time);
	events_register(EVT_DISPLAY, "DISPLAY", 0, MINUTES_(LCD_REFRESH_INTERVAL), &event_update_display);
	events_register(EVT_SMSCHECK, "SMSCHECK", 0, SECONDS_(30), &event_SIM_check_incoming_msgs);
#endif
}
