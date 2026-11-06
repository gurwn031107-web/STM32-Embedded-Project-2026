/*
 * TimeClock.h
 *
 *  Created on: 2026. 10. 19.
 *      Author: rhoblack
 */

#ifndef SRC_AP_TIMECLOCK_TIMECLOCK_H_
#define SRC_AP_TIMECLOCK_TIMECLOCK_H_
#include "stm32f4xx_hal.h"

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
} TimeClock_t;

typedef struct {
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
} TimeDate_t;


void TimeClock_init(void);
TimeClock_t *TimeClock_getTimeData(void);
TimeDate_t *TimeClock_getDateData(void);
void TimeClock_ISR(void);
void TimeClock_task(void);

#endif /* SRC_AP_TIMECLOCK_TIMECLOCK_H_ */
