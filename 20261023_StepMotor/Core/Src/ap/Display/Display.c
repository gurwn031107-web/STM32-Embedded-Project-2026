/*
 * Display.c
 *
 *  Created on: 2026. 10. 20.
 *      Author: rhoblack
 */

#include "Display.h"
#include "../../driver/fnd/fnd.h"
#include "../../driver/led/led.h"
#include "../../driver/lcd/lcd.h"
#include <stdio.h>
#include <string.h>

enum {TIMECLOCK_MODE, STOPWATCH_MODE};
enum {STOPWATCH_RUN, STOPWATCH_STOP};

extern I2C_HandleTypeDef hi2c1;

stopWatchTime_t *pStopWatchTime;
timeClockTime_t *pTimeClockTime;
timeClockDate_t *pTimeClockDate;

led_t ledTimeClock, ledStopWatch;
led_t ledStopWatchRun, ledStopWatchStop;

static timeClockDate_t prevDate = {0};	// LCD 2줄에 마지막으로 쓴 날짜 (스톱워치도 써야 해서 함수 밖으로 뺌)

void Disp_init(void)
{
	led_init(&ledTimeClock, GPIOB, GPIO_PIN_15);
	led_init(&ledStopWatch, GPIOB, GPIO_PIN_14);
	led_init(&ledStopWatchRun, GPIOC, GPIO_PIN_4);
	led_init(&ledStopWatchStop, GPIOB, GPIO_PIN_13);
	Lcd_init(&hi2c1);
}

void Disp_setMode(int mode)
{
	if (mode == TIMECLOCK_MODE){
		led_on(&ledTimeClock);
		led_off(&ledStopWatch);
	}
	else {
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
	num = ((pStopWatchTime->min % 10) * 1000) + (pStopWatchTime->sec * 10) + (pStopWatchTime->msec / 10 % 10);
	fnd_set_num(num);

	if (pStopWatchTime->msec < 50) {
		fnd_set_dp(FND_DIGIT_3, FND_DP_ON);
	}
	else {
		fnd_set_dp(FND_DIGIT_3, FND_DP_OFF);
	}

	fnd_set_dp(FND_DIGIT_1, FND_DP_ON);
	fnd_set_dp(FND_DIGIT_2, FND_DP_OFF);

	// LCD 2줄 : 시:분:초:ms (달력 자리에 스톱워치)
	char stopWatchStr[30];
	sprintf(stopWatchStr, " SW :%02d:%02d:%02d:%02d     ",	// 뒤 공백 5칸 : 달력(16칸)보다 짧아서 남은 글자 지우기
			pStopWatchTime -> hour, pStopWatchTime -> min, pStopWatchTime -> sec, pStopWatchTime -> msec);
	Lcd_writeStringXY(1, 0, stopWatchStr);

	// 2줄을 스톱워치가 덮어썼으므로, 시계 모드로 돌아가면 달력을 다시 그리게 prevDate를 비운다
	memset(&prevDate, 0, sizeof(timeClockDate_t));
}

void Disp_setLcdTimeClockTime(timeClockTime_t *pTime)
{
	pTimeClockTime = pTime;
	Disp_lcdTimeClockTime();
}

// 스톱워치 모드용 : LCD 1줄에 시계만 쓴다 (FND는 스톱워치가 쓰고 있으므로 건드리지 않음)
void Disp_lcdTimeClockTime(void)
{
	char timeStr[30];
	sprintf(timeStr, "%02d:%02d:%02d", pTimeClockTime -> hour, pTimeClockTime -> min, pTimeClockTime -> sec);
	Lcd_writeStringXY(0, 0, timeStr);
}

void Disp_timeClockTime(void)
{
	uint16_t num;
	num = (pTimeClockTime->min * 100) + pTimeClockTime->sec;
	fnd_set_num(num);

	if (pTimeClockTime->msec < 50) {
		fnd_set_dp(FND_DIGIT_2, FND_DP_ON);
	}
	else {
		fnd_set_dp(FND_DIGIT_2, FND_DP_OFF);
	}
	fnd_set_dp(FND_DIGIT_1, FND_DP_OFF);
	fnd_set_dp(FND_DIGIT_3, FND_DP_OFF);

	char timeStr[30];
	sprintf(timeStr, "TIME:%02d:%02d:%02d", pTimeClockTime -> hour, pTimeClockTime -> min, pTimeClockTime -> sec);
	Lcd_writeStringXY(0, 0, timeStr);
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


void Disp_timeClockDate()
{
	if (!memcmp(&prevDate, pTimeClockDate, sizeof(timeClockDate_t)))
	{
		return;
	}
	memcpy(&prevDate, pTimeClockDate, sizeof(timeClockDate_t));

	char dateStr[30];
	const char *strDay[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
	sprintf(dateStr, "20%02d-%02d-%02d [%s]",
			pTimeClockDate -> year, pTimeClockDate -> month,
			pTimeClockDate -> date, strDay[pTimeClockDate -> day - 1]);
	Lcd_writeStringXY(1, 0, dateStr);
}


void Disp_setTimeClockDate(timeClockDate_t *pDate)
{
	pTimeClockDate = pDate;
	Disp_timeClockDate();
}

void Disp_ISR(void)
{
	fnd_show_number();
}














