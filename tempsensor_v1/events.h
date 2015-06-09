/*
 * time_event.h
 *
 *  Created on: Jun 9, 2015
 *      Author: sergioam
 */

#ifndef EVENTS_H_
#define EVENTS_H_

#define MAX_EVENTS 10

typedef struct
__attribute__((__packed__)) {
	char name[16];
	void (*callback)(time_t);
	time_t nextEventRun;
	time_t lastEventRun;
	time_t interval;    // Interval between events in minutes
	time_t startTime; // Minute of the current day to start this event
					  // 0 is now
} EVENT;

typedef struct
__attribute__((__packed__)) {
	EVENT events[MAX_EVENTS];
	uint8_t registeredEvents;
	uint8_t nextEvent;
} EVENT_MANAGER;

void events_find_next_event();
void events_run(time_t);
void events_init();
void events_sync(time_t currentTime);

#endif /* EVENTS_H_ */
