/*
 * Display.c
 *
 *  Created on: 2026. 10. 20.
 *      Author: kccistc
 */

#include "Display.h"
#include "../../driver/fnd/fnd.h"
#include "../../driver/led/led.h"

enum {TIMECLOCK_MODE, STOPWATCH_MODE};
enum {STOPWATCH_RUN, STOPWATCH_STOP};

stopWatchTime_t *pStopWatchTime;
timeClockTime_t *pTimeClockTime;
led_t ledTimeClock, ledStopWatch;
led_t ledStopWatchRun, ledStopWatchStop;

void Disp_init(void)
{
	led_init(&ledTimeClock, GPIOB, GPIO_PIN_15);
	led_init(&ledStopWatch, GPIOB, GPIO_PIN_14);
	led_init(&ledStopWatchRun, GPIOC, GPIO_PIN_4);
	led_init(&ledStopWatchStop, GPIOB, GPIO_PIN_13);
}

void Disp_setMode(int mode)
{
	if (mode == TIMECLOCK_MODE){
		led_on(&ledTimeClock);
		led_off(&ledStopWatch);
	}
	else{
		led_off(&ledTimeClock);
		led_on(&ledStopWatch);
	}
}

void Disp_setTimeClockTime(timeClockTime_t * pTime)
{
	pTimeClockTime = pTime;
	Disp_timeClockTime();
}

void Disp_setStopWatchTime(stopWatchTime_t *pTime)
{
	pStopWatchTime = pTime;
	Disp_stopWatchTime();
}

void Disp_stopWatchTime(void)
{
	uint16_t num;
	num = ((pStopWatchTime -> min % 10) * 1000) + (pStopWatchTime -> sec * 10) + (pStopWatchTime -> msec * 1/10);
	fnd_set_num(num);

	if (pStopWatchTime -> msec < 50) {
		fnd_set_dp(FND_DIGIT_3, FND_DP_ON);
	}
	else {
		fnd_set_dp(FND_DIGIT_3, FND_DP_OFF);
	}
	fnd_set_dp(FND_DIGIT_1, FND_DP_ON);
	fnd_set_dp(FND_DIGIT_2, FND_DP_OFF);
}

void Disp_timeClockTime(void)
{
	uint16_t num;
	num = (pTimeClockTime -> min * 100) + pTimeClockTime -> sec;
	fnd_set_num(num);

	if (pTimeClockTime -> msec < 50) {
		fnd_set_dp(FND_DIGIT_2, FND_DP_ON);
	}
	else {
		fnd_set_dp(FND_DIGIT_2, FND_DP_OFF);
	}
	fnd_set_dp(FND_DIGIT_1, FND_DP_OFF);
	fnd_set_dp(FND_DIGIT_3, FND_DP_OFF);
}

void Disp_setStopWatchState(int state)
{
	if (state == STOPWATCH_RUN){
		led_on(&ledStopWatchRun);
		led_off(&ledStopWatchStop);
	}
	else {
		led_off(&ledStopWatchRun);
		led_on(&ledStopWatchStop);
	}
}

void Disp_ISR(void)
{
	fnd_show_number();
}
