/*
 * timeClock.h
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#ifndef SRC_AP_MAIN_SERVICE_TIMECLOCK_TIMECLOCK_H_
#define SRC_AP_MAIN_SERVICE_TIMECLOCK_TIMECLOCK_H_

void timeClock_setDateFirstFlag(void);
void timeClock_init(void);
void timeClock_task(void);
void timeClock_calendar_task(void);

#endif /* SRC_AP_MAIN_SERVICE_TIMECLOCK_TIMECLOCK_H_ */
