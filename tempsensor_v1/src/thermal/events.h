/*
 * time_event.h
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#ifndef EVENTS_H_
#define EVENTS_H_

#define SECONDS_(s) (s)
#define MINUTES_(m) (m*60L)
#define HOURS_(h)   (h*60L*60L)

// EVENTS
extern int g_iRunning;
extern uint8_t iMainSleep;
extern volatile time_t iSecondTick;

// Resume execution
#define WAKEUP_EVENT if (iMainSleep!=0 && iSecondTick > g_sEvents.events[g_sEvents.nextEvent].nextEventRun) \
		__bic_SR_register_on_exit(LPM0_bits);

#define WAKEUP_MAIN if (iMainSleep!=0) __bic_SR_register_on_exit(LPM0_bits);

#define SYSTEM_RUNNING_CHECK if (!g_iRunning) break;

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
	EVT_PERIODIC_REBOOT,
	EVT_CONFIGURATION,
	EVT_RESET_FAILOVER,
	EVT_LAST
} EVENT_IDS;

#define MAX_EVENTS EVT_LAST

typedef struct {
	EVENT_IDS id;
	char name[8];
	void (*callback)(void *, time_t);
	time_t nextEventRun;
	time_t lastEventRun;
	time_t interval_secs;    // Interval between events in minutes
	time_t offset_secs; // Minute of the current day to start this event
					  // 0 is now
} EVENT;

typedef struct {
	EVENT events[MAX_EVENTS];
	uint8_t registeredEvents;
	uint8_t nextEvent;
	uint8_t init;
} EVENT_MANAGER;

extern EVENT_MANAGER g_sEvents;

time_t event_getIntervalMinutes(EVENT *pEvent);
time_t events_getTick();
void event_main_sleep();
void event_setInterval_by_id_secs(EVENT_IDS id, time_t time_seconds);
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
