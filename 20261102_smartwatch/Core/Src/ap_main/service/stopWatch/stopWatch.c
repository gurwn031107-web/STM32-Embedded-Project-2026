/*
 * stopWatch.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#include "stopWatch.h"
#include "../../../driver/button/button.h"
#include "../../../driver/lcd/lcd.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t msec;
} stopWatch_t;

typedef enum {STOP, RUN, CLEAR} stopWatch_state_t;

extern TIM_HandleTypeDef htim3;

stopWatch_t stopWatch_data = {0};
stopWatch_state_t stopWatch_state = STOP;

bool stopWatch_runFlag = false;

button_t btn_run_stop, btn_clear;

static void stopWatch_incMSec(void)
{
	if (stopWatch_data.msec < 99) {
		stopWatch_data.msec++;
		return;
	}
	stopWatch_data.msec = 0;
	if (stopWatch_data.sec < 59) {
		stopWatch_data.sec++;
		return;
	}
	stopWatch_data.sec = 0;
	if (stopWatch_data.min < 59) {
		stopWatch_data.min++;
		return;
	}
	stopWatch_data.min = 0;
	if (stopWatch_data.hour < 23) {
		stopWatch_data.hour++;
		return;
	}
	stopWatch_data.hour = 0;
}

void stopWatch_disp(void)
{
	char buff[30];
	sprintf(buff, "SW : %02d:%02d:%02d:%02d", stopWatch_data.hour, stopWatch_data.min, stopWatch_data.sec, stopWatch_data.msec);
	Lcd_writeStringXY(1, 0, buff);
}

static void stopWatch_stop(void)
{
	static uint8_t prevMSec = 0;
	static uint8_t prevSec = 0;

	if ((stopWatch_data.msec == prevMSec) && (stopWatch_data.sec == prevSec)) {
		return;
	}
	prevSec	 = stopWatch_data.sec;
	prevMSec = stopWatch_data.msec;

	stopWatch_disp();
}

static void stopWatch_run(void)
{
	if (stopWatch_runFlag != true) {
		return;
	}
	stopWatch_runFlag = false;
	stopWatch_disp();
}

static void stopWatch_clear(void)
{
	stopWatch_data.hour = 0;
	stopWatch_data.min = 0;
	stopWatch_data.sec = 0;
	stopWatch_data.msec = 0;
	stopWatch_disp();
}

void stopWatch_task(void)
{
	switch(stopWatch_state)
	{
	case STOP:
		if (button_getState(&btn_run_stop) == ACT_PUSH) {
			stopWatch_state = RUN;
		}
		stopWatch_stop();
		if (button_getState(&btn_clear) == ACT_RELEASE) {
			stopWatch_state = CLEAR;
		}
		stopWatch_stop();
		break;
	case RUN:
		if (button_getState(&btn_run_stop) == ACT_PUSH) {
			stopWatch_state = STOP;
		}
		stopWatch_run();
		break;
	case CLEAR:
		stopWatch_state = STOP;
		stopWatch_clear();
		break;
	default:
		stopWatch_stop();
		break;

	}
}

void stopWatch_ISR(void)
{
	if (stopWatch_state == RUN) {
		stopWatch_runFlag = true;
		stopWatch_incMSec();
	}
}

void stopWatch_init(void)
{
	button_init(&btn_run_stop, GPIOC, GPIO_PIN_1);
	button_init(&btn_clear, GPIOC, GPIO_PIN_3);
	HAL_TIM_Base_Start_IT(&htim3);
	stopWatch_clear();
}




