/*
 * distance.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#include "distance.h"
#include "../../../driver/HCSR04/HCSR04.h"
#include "../../../driver/button/button.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>
#include <stdbool.h>

extern TIM_HandleTypeDef htim10;

#define DISTANCE_TIMER &htim10
#define TRIG_PORT	GPIOC
#define TRIG_PIN	GPIO_PIN_5
#define ECHO_PORT 	GPIOC
#define ECHO_PIN 	GPIO_PIN_6
#define BTN_DISTANCE_PORT	GPIOC
#define BTN_DISTANCE_PIN 	GPIO_PIN_1

static uint32_t echo_us = 0;
static bool start_flag = false;
static hcsr04_status_t hcsr04_status;
static button_t btn_distance;

void distance_init(void)
{
	HCSR04_init(TRIG_PORT, TRIG_PIN, ECHO_PORT, ECHO_PIN, DISTANCE_TIMER);
	button_init(&btn_distance, BTN_DISTANCE_PORT, BTN_DISTANCE_PIN);
}

static void distance_request(void)
{
	hcsr04_status_t state_start = HCSR04_start();
	if (state_start != HCSR04_ERR_BUSY) {
		start_flag = true;
	}
}

void distance_disp(uint16_t distance)
{
	char buff[30];
	sprintf(buff, "distance:%4d cm", distance);
	Lcd_writeStringXY(1, 0, buff);
}

void distance_task(void)
{
	if (button_getState(&btn_distance) == ACT_RELEASE) {
		distance_request();
	}

	hcsr04_status = HCSR04_getResult(&echo_us);

	if (start_flag != true) {
		return;
	}

	if (hcsr04_status != HCSR04_OK) {
		return;
	}
	start_flag = false;
	uint16_t distance_cm = echo_us * 17 / 1000;
	distance_disp(distance_cm);
}

void distance_ISR(void)
{
	HCSR04_echoISR();
}










