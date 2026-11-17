/*
 * event_task.c
 *
 *  Created on: 2026. 11. 10.
 *      Author: kccistc
 */


#include "event_task.h"
#include "../../../driver/button/button.h"

btn_status_t btn_status; // btn_status = 공유 메모리

button_t btn_run_stop, btn_clear;

void event_task_init(void)
{
	btn_status = BTN_NO;
	button_init(&btn_run_stop, GPIOC, GPIO_PIN_1);
	button_init(&btn_clear, GPIOC, GPIO_PIN_3);
}

void event_task(void)
{
	if (button_getState(&btn_run_stop) == ACT_RELEASE) {
		btn_status = BTN_RUN_STOP;
	}
	else if (button_getState(&btn_clear) == ACT_RELEASE) {
		btn_status = BTN_CLEAR;
	}
}
