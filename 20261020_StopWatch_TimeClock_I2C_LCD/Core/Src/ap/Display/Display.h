/*
 * Display.h
 *
 *  Created on: 2026. 10. 20.
 *      Author: kccistc
 */

#ifndef SRC_AP_DISPLAY_DISPLAY_H_
#define SRC_AP_DISPLAY_DISPLAY_H_

#include <stdint.h>

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
} stopWatchTime_t;

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
} timeClockTime_t;

void Disp_init(void);
void Disp_setMode(int mode);
void Disp_setStopWatchState(int state);
void Disp_setStopWatchTime(stopWatchTime_t *pTime);
void Disp_setTimeClockTime(timeClockTime_t *pTime);
void Disp_stopWatchTime(void);
void Disp_timeClockTime(void);
void Disp_ISR(void);

#endif /* SRC_AP_DISPLAY_DISPLAY_H_ */
