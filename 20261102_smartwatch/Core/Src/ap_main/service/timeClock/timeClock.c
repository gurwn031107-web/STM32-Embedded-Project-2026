/*
 * timeClock.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */


#include "timeClock.h"
#include "../../../driver/DS3231/DS3231.h"
#include "../../../driver/lcd/lcd.h"
#include <stdbool.h>
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

static ds3231_datetime_t rtc_datetime = {
		.date = {.year = 26, .month = 10, .date = 22, .day = 5},
		.time = {.hour = 11, .min = 44, .sec = 30}
};

static ds3231_time_t rtc_time;
static ds3231_date_t rtc_date;

static bool dateFirstFlag = true;

void timeClock_setDateFirstFlag(void)
{
	dateFirstFlag = true;
}

void timeClock_init(void)
{
	ds3231_init(&hi2c1);
	Lcd_init(&hi2c1);
	ds3231_getDateTime(&rtc_datetime);
	ds3231_getTime(&rtc_time);
	ds3231_getDate(&rtc_date);
}

static void timeClock_dispTime(void)
{
	char buff[30];
	sprintf(buff, "Time : %02d:%02d:%02d", rtc_time.hour, rtc_time.min, rtc_time.sec);
	Lcd_writeStringXY(0, 0, buff);
}

static void timeClock_dispDate(void)
{
	char *str_day[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
	char buff[30];

	sprintf(buff, "20%02d-%02d-%02d [%s]  ", rtc_date.year, rtc_date.month, rtc_date.date, str_day[rtc_date.day - 1]);
	Lcd_writeStringXY(1, 0, buff);
}

void timeClock_task(void)
{
	static uint32_t prevTime = 0;
	if (HAL_GetTick() - prevTime < 1000) {
		return;
	}
	prevTime = HAL_GetTick();

	ds3231_getTime(&rtc_time);
	timeClock_dispTime();
}

void timeClock_calendar_task(void)
{
	static uint32_t prevTime = 0;

	if (dateFirstFlag == true) {
		dateFirstFlag = false;

		ds3231_getDate(&rtc_date);
		timeClock_dispDate();

		return;
	}

	if (HAL_GetTick() - prevTime < 3600000 - 1) {
		return;
	}
	prevTime = HAL_GetTick();

	ds3231_getDate(&rtc_date);
	timeClock_dispDate();
}






