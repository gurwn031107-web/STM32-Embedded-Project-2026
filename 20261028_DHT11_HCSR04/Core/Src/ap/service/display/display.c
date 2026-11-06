/*
 * display.c
 *
 *  Created on: 2026. 10. 28.
 *      Author: kccistc
 *
 *  LCD 1줄: 초음파 거리 (200ms마다 갱신)
 *  LCD 2줄: 온도/습도   (1초마다 갱신)
 */


#include "display.h"
#include "../TempHumid/TempHumid.h"
#include "../Distance/Distance.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

void display_init(void)
{
	char buff[30];
	Lcd_init(&hi2c1);
	sprintf(buff, "Hello STM32!");
	Lcd_writeStringXY(0, 0, buff);
}

static void display_distance(void)
{
	char buff[30];
	uint32_t mm;
	hcsr04_status_t s = Distance_get(&mm);

	// 16칸을 꽉 채워서 이전 글자가 남지 않게 한다
	switch (s)
	{
	case HCSR04_OK:
		sprintf(buff, "Dist: %3lu.%lu cm  ", mm / 10, mm % 10);
		break;
	case HCSR04_ERR_OUT_OF_RANGE:
		sprintf(buff, "Dist: Out range ");
		break;
	case HCSR04_ERR_NO_ECHO:
		sprintf(buff, "Dist: No echo   ");
		break;
	default:
		sprintf(buff, "Dist: ---.- cm  ");
		break;
	}
	Lcd_writeStringXY(0, 0, buff);
}

static void display_tempHumid(void)
{
	char buff[30];
	dht11_data_t thData;

	TH_getTempHumid(&thData);
	sprintf(buff, "T: %2u.%u H: %2u%%", thData.t_int, thData.t_dec, thData.h_int);
	Lcd_writeStringXY(1, 0, buff);
}

void display_task(void)
{
	static uint32_t prevDist = 0;
	static uint32_t prevTH = 0;

	if (HAL_GetTick() - prevDist >= 200) {
		prevDist = HAL_GetTick();
		display_distance();
	}

	if (HAL_GetTick() - prevTH >= 1000) {
		prevTH = HAL_GetTick();
		display_tempHumid();
	}
}
