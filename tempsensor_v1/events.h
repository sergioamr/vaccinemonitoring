/*
 * time_event.h
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#ifndef EVENTS_H_
#define EVENTS_H_

extern uint8_t iMainSleep;
extern volatile time_t iSecondTick;
#define event_wakeup_main (iMainSleep!=0 && iSecondTick > g_sEvents.events[g_sEvents.nextEvent].nextEventRun)

typedef enum {
	EVT_DISPLAY = 0,
	EVT_PULLTIME,
	EVT_SMSCHECK,
	EVT_SUBSAMPLE_TEMP,
	EVT_SAVE_SAMPLE_TEMP,
	EVT_SMS_TEST,
	EVT_CHECK_NETWORK,
	EVT_LCD_OFF,
	EVT_UPLOAD_SAMPLES,
	EVT_BATTERY_CHECK,
	EVT_ALARMS_CHECK,
	EVT_LAST
} EVENT_IDS;

#define MAX_EVENTS EVT_LAST

typedef struct
__attribute__((__packed__)) {
	EVENT_IDS id;
	char name[16];
	void (*callback)(void *, time_t);
	time_t nextEventRun;
	time_t lastEventRun;
	time_t intervalDefault;    // Interval between events in minutes
	time_t interval;		   // Interval pointer
	time_t startTime; // Minute of the current day to start this event
					  // 0 is now
} EVENT;

typedef struct
__attribute__((__packed__)) {
	EVENT events[MAX_EVENTS];
	uint8_t registeredEvents;
	uint8_t nextEvent;
	uint8_t init;
} EVENT_MANAGER;

extern EVENT_MANAGER g_sEvents;

time_t events_getTick();
void event_main_sleep();
void event_setInterval_by_id(EVENT_IDS id, time_t time);
void events_send_data(char *phone);
void events_debug();
void events_find_next_event();
void events_run();
void events_init();
void events_sync();
void event_init(EVENT *pEvent, time_t currentTime);
void event_force_event_by_id(EVENT_IDS id, time_t offset);
void event_run_now_by_id(EVENT_IDS id);

void event_LCD_turn_on();
EVENT *events_find(EVENT_IDS id);

#endif /* EVENTS_H_ */
