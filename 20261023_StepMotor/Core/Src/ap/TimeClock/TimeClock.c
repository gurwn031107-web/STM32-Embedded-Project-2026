/*
 * TimeClock.c
 *
 *  Created on: 2026. 10. 19.
 *      Author: rhoblack
 */
#include "TimeClock.h"
#include "../../driver/fnd/fnd.h"
#include "../../driver/DS3231/DS3231.h"

extern I2C_HandleTypeDef hi2c1;

static TimeClock_t timeClockData; // 중심 데이터
static TimeDate_t timeDateData;


ds3231_datetime_t rtc_datetime = {
		.date = {.year = 26, .month = 10, .date = 22, .day = 5},
		.time = {.hour = 11, .min = 44, .sec = 30}
};

ds3231_time_t rtc_time;
ds3231_date_t rtc_date;

void TimeClock_init(void)
{
	ds3231_init(&hi2c1);
	ds3231_getDateTime(&rtc_datetime);
	ds3231_getTime(&rtc_time);
	ds3231_getDate(&rtc_date);

	timeClockData.hour = rtc_datetime.time.hour;
	timeClockData.min  = rtc_datetime.time.min;
	timeClockData.sec  = rtc_datetime.time.sec;
	timeClockData.msec = 0;
}

TimeClock_t *TimeClock_getTimeData(void)
{
	timeClockData.hour = rtc_time.hour;
	timeClockData.min  = rtc_time.min;
	timeClockData.sec  = rtc_time.sec;
	return &timeClockData;
}

TimeDate_t *TimeClock_getDateData(void)
{
	timeDateData.year  = rtc_date.year;
	timeDateData.month = rtc_date.month;
	timeDateData.date  = rtc_date.date;
	timeDateData.day   = rtc_date.day;

	return &timeDateData;
}

static void TimeClock_incMSec(void)
{
	// 시 · 분 · 초는 DS3231(RTC)이 기준 → 여기서는 msec(FND 점 깜빡임용)만 센다
	if (timeClockData.msec < 99) {
		timeClockData.msec++;
		return;
	}
	timeClockData.msec = 0;
}

void TimeClock_ISR(void)
{
	TimeClock_incMSec();
}

void TimeClock_task(void)
{
	static uint32_t prevTick = 0;
	static uint8_t prevHour = 0xff;

	// 100ms마다 RTC를 읽는다 (STM32 초에 맞춰 읽으면, RTC 초와 타이밍이 겹칠 때 초가 건너뛰거나 멈춤)
	if (HAL_GetTick() - prevTick < 100) {
		return;
	}
	prevTick = HAL_GetTick();
	ds3231_getTime(&rtc_time);

	if (rtc_time.hour == prevHour) {
		return;
	}
	prevHour = rtc_time.hour;
	ds3231_getDate(&rtc_date);

	//TimeClock_dispTime();
}



