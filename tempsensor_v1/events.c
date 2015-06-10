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

	nextEvent = 0;

	// Search for the event that is closer in time
	for (t = 0; t < MAX_EVENTS; t++) {
		pEvent = &g_sEvents.events[t];
		if (pEvent->id!=EVT_EMPTY && pEvent->nextEventRun < nextEventTime) {
			nextEventTime = pEvent->nextEventRun;
			nextEvent = t;
		}
	}

	g_sEvents.nextEvent = nextEvent;
}

size_t event_getInterval(EVENT *pEvent) {
	return *pEvent->pInterval;
}

// The interval can be dynamic
void events_register(EVENT_IDS id, char *name, time_t startTime,
		void (*functionCall)(void *, time_t), time_t intervalDefault,
		time_t *pInterval) {
	EVENT *pEvent;

	// Not enough space to register an event.
	if (id >= MAX_EVENTS)
		return;

	pEvent = &g_sEvents.events[id];
	pEvent->id = id;
	strncpy(pEvent->name, name, sizeof(pEvent->name));

	pEvent->intervalDefault = intervalDefault;
	if (pInterval == NULL)
		pEvent->pInterval = &pEvent->intervalDefault;
	else
		pEvent->pInterval = pInterval;

	pEvent->lastEventRun = 0;
	pEvent->nextEventRun = 0;
	pEvent->startTime = startTime;
	pEvent->callback = functionCall;
}

void event_init(EVENT *pEvent, time_t currentTime) {
	if (pEvent->id==EVT_EMPTY)
		return;

	pEvent->nextEventRun = currentTime + pEvent->startTime + *pEvent->pInterval;
}

void event_next(EVENT *pEvent, time_t currentTime) {
	pEvent->lastEventRun = pEvent->nextEventRun;
	pEvent->nextEventRun = currentTime + *pEvent->pInterval;
}

void events_sync(time_t currentTime) {
	EVENT *pEvent;
	int t;

	for (t = 0; t < MAX_EVENTS; t++) {
		pEvent = &g_sEvents.events[t];
		event_init(pEvent, currentTime);
	}
	events_find_next_event(0);
}

void events_debug(time_t currentTime) {
#ifdef _DEBUG
	EVENT *pEvent = &g_sEvents.events[g_sEvents.nextEvent];
	if (pEvent->id == EVT_DISPLAY)
		return;

	time_t nextEventTime = pEvent->nextEventRun - currentTime;
	int test = nextEventTime % 10;
	if (test == 0 || nextEventTime < 10)
		lcd_printf(LINE2, "[%s] %d  ", pEvent->name, nextEventTime);
#endif
}

void event_run_now(EVENT *pEvent, time_t currentTime) {

	// Disable hardware interruptions before running the actions
	hardware_disable_buttons();

	// Store in the log file
	config_save_command(pEvent->name);

	// Move the interval to the next time
	event_next(pEvent, currentTime);
	pEvent->callback(pEvent, currentTime);
	events_find_next_event(currentTime);

	hardware_enable_buttons();
}

void event_run_now_by_id(EVENT_IDS id) {

	EVENT *pEvent = events_find(id);
	if (pEvent == NULL)
		return;

	event_run_now(pEvent, thermal_update_time());
}

void event_force_event_by_id(EVENT_IDS id, time_t offset) {

	time_t currentTime;
	EVENT *pEvent = events_find(id);
	if (pEvent == NULL)
		return;

	currentTime = thermal_update_time();
	pEvent->nextEventRun = currentTime + offset;
	events_find_next_event(currentTime);
}

void events_run(time_t currentTime) {
	EVENT *pEvent;
	uint8_t nextEvent = g_sEvents.nextEvent;

	if (nextEvent > MAX_EVENTS)
		return;

	pEvent = &g_sEvents.events[nextEvent];
	while (currentTime >= pEvent->nextEventRun && pEvent!=NULL) {
		event_run_now(pEvent, currentTime);
		pEvent = &g_sEvents.events[nextEvent];
	}
}

/*******************************************************************************************************/
/* Event functions */
/*******************************************************************************************************/

void event_sms_test(void *event, time_t currentTime) {
	sms_send_data_request(LOCAL_TESTING_NUMBER);
	event_force_event_by_id(EVT_DISPLAY, 0);
}

void event_SIM_check_incoming_msgs(void *event, time_t currentTime) {

	//TODO process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
	EVENT *pEvent = (EVENT *) event;
	sms_process_messages();
	if (g_pDevCfg->cfgServerConfigReceived == 1) {
		*pEvent->pInterval = MINUTES_(MSG_REFRESH_INTERVAL);
		pEvent->nextEventRun = pEvent->nextEventRun + *pEvent->pInterval;
	}
}

void event_pull_time(void *event, time_t currentTime) {
	modem_pull_time();
	// We update all the timers according to the new time
	events_sync(thermal_update_time());
}

void event_update_display(void *event, time_t currentTime) {
	config_update_system_time();
	// Invalidate display
	lcd_show();
}

void event_display_off(void *event, time_t currentTime) {
	lcd_turn_off();
}

// modem_check_network(); // Checks network and if it is down it does the swapping
void event_sample_temperature(void *event, time_t currentTime) {
	UINT bytes_written = 0;
	temperature_sample();
	log_sample_web_format(&bytes_written);
}

void event_upload_samples(void *event, time_t currentTime) {
}

EVENT *events_find(EVENT_IDS id) {
	EVENT *pEvent = &g_sEvents.events[id];
	if (pEvent->id!=id)
		return NULL;

	return pEvent;
}

// turns on the screen, resets the timeout to turn it off
void event_LCD_turn_on() {
	EVENT *event = events_find(EVT_LCD_OFF);
	if (event == NULL)
		return;

	lcd_turn_on();
	event_init(event, thermal_update_time());
}

void events_init() {
	memset(&g_sEvents, 0, sizeof(g_sEvents));

#ifdef _DEBUG
	events_register(EVT_SMS_TEST, "SMS_TEST", 0, &event_sms_test, MINUTES_(10),
			NULL);
#endif
	events_register(EVT_PULLTIME, "PULLTIME", 0, &event_pull_time, HOURS_(12),
			NULL);

	events_register(EVT_CHECK_NETWORK, "NET CHECK", 1,
			&event_sample_temperature, MINUTES_(SAMPLE_PERIOD), NULL);

	// Check every 30 seconds until we get the configuration message from server;
	events_register(EVT_SMSCHECK, "SMSCHECK", 0, &event_SIM_check_incoming_msgs,
			SECONDS_(30), NULL);

	events_register(EVT_LCD_OFF, "LCD OFF", 1, &event_display_off, MINUTES_(10),
			NULL);

	events_register(EVT_DISPLAY, "DISPLAY", 0, &event_update_display,
			MINUTES_(LCD_REFRESH_INTERVAL), NULL);

	events_register(EVT_SAMPLE_TEMP, "SAMPLE TMP", 0, &event_sample_temperature,
			MINUTES_(SAMPLE_PERIOD),
			&g_pDevCfg->stIntervalParam.loggingInterval);

	events_register(EVT_UPLOAD_SAMPLES, "UPLOAD", 0, &event_upload_samples,
			MINUTES_(UPLOAD_PERIOD),
			&g_pDevCfg->stIntervalParam.uploadInterval);
}
