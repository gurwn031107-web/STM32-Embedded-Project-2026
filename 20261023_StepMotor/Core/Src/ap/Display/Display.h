/*
 * Display.h
 *
 *  Created on: 2026. 10. 20.
 *      Author: rhoblack
 */

#ifndef SRC_AP_DISPLAY_DISPLAY_H_
#define SRC_AP_DISPLAY_DISPLAY_H_

#include <stdint.h>

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
}stopWatchTime_t;

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
}timeClockTime_t;

typedef struct {
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
}timeClockDate_t;

void Disp_init(void);
void Disp_setMode(int mode);
void Disp_setTimeClockTime(timeClockTime_t * pTime);
void Disp_timeClockTime(void);
void Disp_setStopWatchTime(stopWatchTime_t *pTime);
void Disp_stopWatchTime(void);
void Disp_setStopWatchState(int state);
void Disp_ISR(void);
void Disp_setTimeClockDate(timeClockDate_t *pDate);
void Disp_timeClockDate();
void Disp_setLcdTimeClockTime(timeClockTime_t *pTime);
void Disp_lcdTimeClockTime(void);




#endif /* SRC_AP_DISPLAY_DISPLAY_H_ */
