/*
 * tempHumid.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#include "tempHumid.h"
#include "../../../driver/DHT11/DHT11.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>

#define dht_port    GPIOC
#define dht_pin     GPIO_PIN_8

static dht11_data_t tempHumid_data;

void tempHumid_init(void)
{
	dht11_init(dht_port, dht_pin);
}

static void tempHumid_disp()
{
	char buff[30];
	sprintf(buff, "T:%02d.%1d C H:%02d %% ", tempHumid_data.t_int, tempHumid_data.t_dec, tempHumid_data.h_int);
	Lcd_writeStringXY(1, 0, buff);
}

void tempHumid_task(void)
{
	static uint32_t prevTime = 0;

	if(HAL_GetTick() - prevTime < 2000) {
		return;
	}
	prevTime = HAL_GetTick();
	dht11_read(&tempHumid_data);
	tempHumid_disp();
}















