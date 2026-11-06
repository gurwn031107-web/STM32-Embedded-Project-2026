/*
 * TimeClock.h
 *
 *  Created on: 2026. 10. 19.
 *      Author: kccistc
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

void TimeClock_init(void);
void TimeClock_ISR(void);
void TimeClock_task(void);
TimeClock_t * TimeClock_getTimeData(void);

#endif /* SRC_AP_TIMECLOCK_TIMECLOCK_H_ */
