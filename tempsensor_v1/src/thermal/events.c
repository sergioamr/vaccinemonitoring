/*
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#include "thermalcanyon.h"
#include "main_system.h"
#include "hardware_buttons.h"
#include "data_transmit.h"
#include "sms.h"
#include "fatdata.h"
#include "modem.h"
#include "state_machine.h"
#include "alarms.h"
#include "timer.h"

#pragma SET_DATA_SECTION(".xbigdata_vars")
EVENT_MANAGER g_sEvents;
#pragma SET_DATA_SECTION()

/*******************************************************************************************************/
/* Event based system */
/*******************************************************************************************************/

time_t inline event_getIntervalSeconds(EVENT *pEvent) {
	if (pEvent == NULL)
		return UINT32_MAX;

	return pEvent->interval_secs + pEvent->offset_secs;
}

time_t event_getIntervalMinutes(EVENT *pEvent) {
	return (event_getIntervalSeconds(pEvent) / 60);
}

void inline event_setIntervalSeconds(EVENT *pEvent, time_t time_seconds) {
	if (pEvent == NULL)
		return;

	// Intervals more than reset time will not be accepted.
	if (time_seconds)
		return;

	pEvent->interval_secs = time_seconds;
}

void event_setInterval_by_id_secs(EVENT_IDS id, time_t time_seconds) {
	EVENT *pEvent = events_find(id);
	if (pEvent == NULL)
		return;

	event_setIntervalSeconds(pEvent, time_seconds);
}

time_t events_getTick() {
#ifdef USING_REALTIME_CLOCK
	static time_t oldTime = 0;
	static time_t frameDiff = 0;
	static time_t elapsedTime = 0;

	time_t newTime = rtc_get_second_tick();
	if (oldTime == 0) {
		oldTime = newTime;
		return 0;
	}

	frameDiff = (newTime - oldTime);

	if (frameDiff < 10000) {
		elapsedTime += frameDiff;
	} else {
		_NOP();
	}

	oldTime = newTime;
	return elapsedTime;
#else
	return rtc_get_second_tick();
#endif
}

void events_send_data(char *phone) {
/*
	char msg[MAX_SMS_SIZE_FULL];
	EVENT *pEvent;
	int t;
	size_t length;

	if (g_sEvents.registeredEvents == 0)
		return;

	size_t currentTime = events_getTick();
	sprintf(msg, "[%s] Events start",
			get_date_string(&g_tmCurrTime, "-", " ", ":", 1));
	sms_send_message_number(phone, msg);

	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		pEvent = &g_sEvents.events[t];
		length = (pEvent->nextEventRun - currentTime);

		sprintf(msg, "%s : left %d next %d", pEvent->name, (uint16_t) length,
				(uint16_t) event_getIntervalMinutes(pEvent));
		sms_send_message_number(phone, msg);
	}

	sms_send_data_request(phone);

	sprintf(msg, "[%s] Events end",
			get_date_string(&g_tmCurrTime, "-", " ", ":", 1));
	sms_send_message_number(phone, msg);
*/
}

/*
void event_sanity_check(EVENT *pEvent, time_t currentTime) {

	time_t maxRunTime = currentTime + pEvent->offset_secs
			+ event_getIntervalSeconds(pEvent);

	if (pEvent->nextEventRun <= maxRunTime)
		return;

	event_init(pEvent, currentTime);
	_NOP();
}

void events_sanity(time_t currentTime) {
	int t;
	EVENT *pEvent = NULL;

	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		pEvent = &g_sEvents.events[t];
		event_sanity_check(pEvent, currentTime);
	}
}
*/

// Populates the next event index depending on event times
void events_find_next_event(time_t currentTime) {
	int t;
	EVENT *pEvent = NULL;
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

	_NOP();
}

// The interval can be dynamic
// We offset the events to allow events to not run simultaneously.
// For example: Subsampling always have to ocurr before Sampling and saving.

void events_register(EVENT_IDS id, char *name, time_t offset_time_secs,
		void (*functionCall)(void *, time_t), time_t intervalDefault,
		time_t intervalMinutes) {
	EVENT *pEvent;
	uint8_t nextEvent;

	// Not enough space to register an event.
	if (g_sEvents.registeredEvents >= MAX_EVENTS)
		return;

	nextEvent = g_sEvents.registeredEvents;

	pEvent = &g_sEvents.events[nextEvent];
	pEvent->id = id;
	strncpy(pEvent->name, name, sizeof(pEvent->name));

#ifdef _DEBUG
	intervalDefault/=2;
	offset_time_secs/=2;
	intervalMinutes/=2;
#endif

	if (intervalDefault != 0)
		pEvent->interval_secs = intervalDefault - offset_time_secs;

	if (intervalMinutes != 0)
		pEvent->interval_secs = MINUTES_(intervalMinutes) - offset_time_secs;

	pEvent->lastEventRun = 0;
	pEvent->nextEventRun = 0;
	pEvent->offset_secs = offset_time_secs;
	pEvent->callback = functionCall;
	g_sEvents.registeredEvents++;
	event_init(pEvent, 0);
}

void event_init(EVENT *pEvent, time_t currentTime) {
	pEvent->nextEventRun = currentTime + pEvent->offset_secs
			+ event_getIntervalSeconds(pEvent);
}

void event_next(EVENT *pEvent, time_t currentTime) {
	pEvent->lastEventRun = pEvent->nextEventRun;
	pEvent->nextEventRun = currentTime + event_getIntervalSeconds(pEvent)
			+ pEvent->offset_secs;
}

void events_sync_rtc() {
	// Events dependent on time
}

void events_sync() {
	EVENT *pEvent;
	int t;

	time_t currentTime = events_getTick();

	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		pEvent = &g_sEvents.events[t];
		event_init(pEvent, currentTime);
	}
	events_find_next_event(0);
}

void events_debug() {
#ifdef _DEBUG
	if (!g_iDebug)
		return;

	time_t currentTime = events_getTick();

	EVENT *pEvent = &g_sEvents.events[g_sEvents.nextEvent];
	//if (pEvent->id == EVT_DISPLAY)
	//	return;

	time_t nextEventTime = pEvent->nextEventRun - currentTime;
	int test = nextEventTime % 10;
	if (test == 0 || nextEventTime < 10 || 1)
		lcd_printf(LINE2, "[%s] %d  ", pEvent->name, nextEventTime);
#endif
}

void event_run_now(EVENT *pEvent) {

	time_t currentTime = events_getTick();

	// Disable hardware interruptions before running the actions
	hardware_disable_buttons();

	// Store in the log file
	config_setLastCommand(pEvent->id);

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

	event_run_now(pEvent);
}

void event_force_event_by_id(EVENT_IDS id, time_t offset) {

	time_t currentTime;
	EVENT *pEvent = events_find(id);
	if (pEvent == NULL)
		return;

	currentTime = events_getTick();
	pEvent->nextEventRun = currentTime + offset;
	events_find_next_event(currentTime);
}

EVENT inline *events_getNextEvent() {
	return &g_sEvents.events[g_sEvents.nextEvent];
}

void events_run() {
	time_t currentTime;
	EVENT *pEvent;
	uint8_t nextEvent = g_sEvents.nextEvent;

	if (nextEvent > MAX_EVENTS)
		return;

	currentTime = events_getTick();

	pEvent = &g_sEvents.events[nextEvent];
	while (currentTime >= pEvent->nextEventRun && pEvent != NULL) {
		if (g_iDebug)
			buzzer_feedback_value(5);

		event_run_now(pEvent);
		nextEvent = g_sEvents.nextEvent;
		pEvent = &g_sEvents.events[nextEvent];
		currentTime = events_getTick();
	}

	//events_sanity(currentTime);
}

/*******************************************************************************************************/
/* Event functions */
/*******************************************************************************************************/

void event_sms_test(void *event, time_t currentTime) {
	if (!g_pDevCfg->cfg.logs.sms_reports)
		return;

	sms_send_data_request(g_pDevCfg->cfgReportSMS);
}

void event_SIM_check_incoming_msgs(void *event, time_t currentTime) {

	//TODO process SMS messages if there is a gap of 2 mins before cfg processing or upload takes place
	EVENT *pEvent = (EVENT *) event;
	sms_process_messages();
	if (g_pDevCfg->cfgServerConfigReceived == 1) {
#ifndef _DEBUG
		event_setIntervalSeconds(pEvent, MINUTES_(PERIOD_SMS_CHECK));
#endif
		pEvent->nextEventRun = pEvent->nextEventRun
				+ event_getIntervalSeconds(pEvent);
	}

	event_force_event_by_id(EVT_DISPLAY, 0);
}

void event_pull_time(void *event, time_t currentTime) {
	modem_pull_time();
	// We update all the timers according to the new time
	events_sync_rtc();
	event_force_event_by_id(EVT_DISPLAY, 0);
}

void event_update_display(void *event, time_t currentTime) {
	// xxx Does system time do this?
	config_update_system_time();
	// Invalidate display
	lcd_show();
}

void event_display_off(void *event, time_t currentTime) {
	lcd_turn_off();
}

void event_save_samples(void *event, time_t currentTime) {
	UINT bytes_written = 0;

	// Push subsampling in the future to align with saving
	EVENT *subsampling = events_find(EVT_SUBSAMPLE_TEMP);
	event_next(subsampling, currentTime);

	log_sample_to_disk(&bytes_written);
	log_sample_web_format(&bytes_written);

	config_setLastCommand(COMMAND_MONITOR_ALARM);

	//monitor for temperature alarms
	alarm_monitor();
	event_force_event_by_id(EVT_DISPLAY, 0);
}

void event_subsample_temperature(void *event, time_t currentTime) {
	lcd_print_progress();
	temperature_subsamples(NUM_SAMPLES_CAPTURE);
	temperature_single_capture();
}

void event_network_check(void *event, time_t currentTime) {
	int res;
	uint8_t *failures;
	int service;

	switch (g_pSysState->lastTransMethod) {
		case HTTP_SIM1:
		case SMS_SIM1:
			res = modem_swap_to_SIM(0);
			break;
		case HTTP_SIM2:
		case SMS_SIM2:
			res = modem_swap_to_SIM(1);
			break;
		case NONE:
		default:
			modem_run_failover_sequence();
			break;
	}

	service = modem_getNetworkService();
	failures = &g_pSysState->net_service[service].network_failures;

	if (state_isNetworkRegistered()) {
		event_setInterval_by_id_secs(EVT_CHECK_NETWORK, MINUTES_(10));
	} else {
		// Try to connect in 1 minute
		event_setInterval_by_id_secs(EVT_CHECK_NETWORK, MINUTES_(1));
		event_force_event_by_id(EVT_DISPLAY, 0);
		return;
	}

	modem_getSignal();
	if (state_isSignalInRange()) {
		res = modem_connect_network(1);
	} else {
		res = UART_FAILED;
	}

	if (res == UART_FAILED) {
		// No signal on this SIM
		g_pSysState->lastTransMethod = NONE;
		*failures++;
		log_appendf("[%d] NETDOWN %d", config_getSelectedSIM(), *failures);
	} else {
		*failures = 0;
	}

	event_force_event_by_id(EVT_DISPLAY, 0);
}

void event_upload_samples(void *event, time_t currentTime) {
	process_batch();
}

EVENT *events_find(EVENT_IDS id) {
	int t;

	for (t = 0; t < g_sEvents.registeredEvents; t++) {
		EVENT *pEvent = &g_sEvents.events[t];
		if (pEvent->id == id)
			return pEvent;
	}

	return NULL;
}

// turns on the screen, resets the timeout to turn it off
void event_LCD_turn_on() {
	EVENT *event = events_find(EVT_LCD_OFF);
	if (event == NULL)
		return;
	lcd_turn_on();
	event_init(event, events_getTick());
}

void events_check_battery(void *event, time_t currentTime) {
	state_battery_level(batt_getlevel());
}

void events_health_check(void *event, time_t currentTime) {
	state_process();
}

void event_reboot_system(void *event, time_t currentTime) {
	system_reboot("Scheduled");
}

void event_fetch_configuration(void *event, time_t currentTime) {
	backend_get_configuration();
}

void event_reset_trans(void *event, time_t currentTime) {
	g_pSysState->lastTransMethod = NONE;
}

// Sleeping state
uint8_t iMainSleep = 0;

// Resume execution if the device is in deep sleep mode
// Triggered by the interruption
uint8_t event_wake_up_main() {
	if (iMainSleep == 0)
		return 0;

	EVENT *pNext = events_getNextEvent();

	if (events_getTick() > pNext->nextEventRun)
		return 1;

	return 0;
}

// Main sleep, if there are not events happening it will just sleep

void event_main_sleep() {
	iMainSleep = 1;

	if (g_bLCD_state)
		delay(MAIN_SLEEP_TIME);
	else
		delay(MAIN_LCD_OFF_SLEEP_TIME);

	iMainSleep = 0;
}

void events_init() {

	memset(&g_sEvents, 0, sizeof(g_sEvents));

#ifdef _DEBUG
	events_register(EVT_SMS_TEST, "SMS_TEST", 0, &event_sms_test,
			MINUTES_(PERIOD_SMS_TEST),
			NULL);
#endif
	events_register(EVT_BATTERY_CHECK, "BATTERY", 0, &events_check_battery,
			MINUTES_(PERIOD_BATTERY_CHECK), g_pDevCfg->sIntervalsMins.batteryCheck);

	events_register(EVT_PULLTIME, "PULLTIME", 0, &event_pull_time,
			MINUTES_(PERIOD_PULLTIME), g_pDevCfg->sIntervalsMins.modemPullTime);

	events_register(EVT_CHECK_NETWORK, "NET CHECK", 1, &event_network_check,
			MINUTES_(PERIOD_NETWORK_CHECK),  g_pDevCfg->sIntervalsMins.networkCheck);

	// Check every 30 seconds until we get the configuration message from server;
	events_register(EVT_SMSCHECK, "SMS CHECK", 0, &event_SIM_check_incoming_msgs,
			MINUTES_(PERIOD_SMS_CHECK), g_pDevCfg->sIntervalsMins.smsCheck);

	events_register(EVT_LCD_OFF, "LCD OFF", 1, &event_display_off,
			MINUTES_(PERIOD_LCD_OFF), NULL);

	events_register(EVT_ALARMS_CHECK, "ALARMS", 1, &events_health_check,
			MINUTES_(PERIOD_ALARMS_CHECK),  g_pDevCfg->sIntervalsMins.alarmsCheck);

	events_register(EVT_DISPLAY, "DISPLAY", 0, &event_update_display,
			MINUTES_(PERIOD_LCD_REFRESH), NULL);

	events_register(EVT_SUBSAMPLE_TEMP, "SUBSAMP", 0,
			&event_subsample_temperature, MINUTES_(PERIOD_SAMPLING),
			g_pDevCfg->sIntervalsMins.sampling);

	// Offset the save N seconds from the subsample taking
	events_register(EVT_SAVE_SAMPLE_TEMP, "SAVE TMP", 15, &event_save_samples,
			MINUTES_(PERIOD_SAMPLING), g_pDevCfg->sIntervalsMins.sampling);

	events_register(EVT_UPLOAD_SAMPLES, "UPLOAD", 0, &event_upload_samples,
			MINUTES_(PERIOD_UPLOAD), g_pDevCfg->sIntervalsMins.upload);

	events_register(EVT_PERIODIC_REBOOT, "REBOOT", 0, &event_reboot_system,
			MINUTES_(PERIOD_REBOOT), g_pDevCfg->sIntervalsMins.systemReboot);

	events_register(EVT_CONFIGURATION, "CONFIG", 30, &event_fetch_configuration,
			MINUTES_(PERIOD_CONFIGURATION_FETCH),
			g_pDevCfg->sIntervalsMins.configurationFetch);

	events_register(EVT_RESET_FAILOVER, "RESET TRANS", 0, &event_reset_trans,
			MINUTES_(PERIOD_TRANS_RESET), g_pDevCfg->sIntervalsMins.transmissionReset);

	events_sync();
}

