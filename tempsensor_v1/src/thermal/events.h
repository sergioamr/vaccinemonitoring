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
	EVT_DISPLAY_ALARM,
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
	char name[4];
	void (*callback)(void *, time_t);
	uint32_t nextEventRun;
	uint32_t lastEventRun;
	uint32_t interval_secs; // Interval between events in seconds, 0 seconds events are disabled events
	uint16_t offset_secs;   // Seconds to offset the start of this event
} EVENT;

// One time commands that will run when the state machine runs.
// Used to defer commands
typedef union {
	struct {
		unsigned char swap_sim :1;
		unsigned char display_config :1;
		unsigned char display_http_error :1;
	} command;
	unsigned char status;
} DEFER_COMMANDS;

typedef struct {
	DEFER_COMMANDS defer;

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
void events_display_alarm(void *event, time_t currentTime) ;
void events_sync_rtc();
void events_sync();
void event_init(EVENT *pEvent, time_t currentTime);
void event_force_event_by_id(EVENT_IDS id, time_t offset);
void event_run_now_by_id(EVENT_IDS id);

void event_LCD_turn_on();
EVENT *events_find(EVENT_IDS id);

#endif /* EVENTS_H_ */
