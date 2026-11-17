/*
 * motor_control_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */

#include "motor_control_task.h"
#include "cmsis_os.h"
#include "../../../driver/dc_motor/dc_motor.h"
#include "../../message.h"

typedef enum {STOP, RUN, LEFT, RIGHT} motor_state_t;

extern TIM_HandleTypeDef htim3;


static motor_t motor_l, motor_r;
static motor_state_t motor_state = STOP;
static btn_status_t btn_status = BTN_NO;

void motor_control_task_init(void)
{
	motor_init(&motor_r, &htim3, TIM_CHANNEL_3, GPIOA, GPIO_PIN_5, GPIOA, GPIO_PIN_6);
	motor_init(&motor_l, &htim3, TIM_CHANNEL_1, GPIOB, GPIO_PIN_6, GPIOA, GPIO_PIN_7);
	motor_set_speed(900);
}

void motor_control_task(void)
{
	osEvent evt;

	evt = osMessageGet(btnQueHandle, 100);
	if (evt.status == osEventMessage) {
		btn_status = evt.value.v;
	}
	else {
		btn_status = BTN_NO;
	}

	switch(motor_state)
	{
	case STOP:
		if (btn_status == BTN_RUN_STOP) {
			motor_state = RUN;
		}
		else if (btn_status == BTN_LEFT) {
			motor_state = LEFT;
		}
		else if (btn_status == BTN_RIGHT) {
			motor_state = RIGHT;
		}
		motor_stop(&motor_l, &motor_r);
		break;
	case RUN:
		if (btn_status == BTN_RUN_STOP) {
			motor_state = STOP;
		}
		else if (btn_status == BTN_LEFT) {
			motor_state = LEFT;
		}
		else if (btn_status == BTN_RIGHT) {
			motor_state = RIGHT;
		}
		motor_forward(&motor_l, &motor_r);
		break;
	case LEFT:
		if (btn_status == BTN_RUN_STOP) {
			motor_state = STOP;
		}
		else if (btn_status == BTN_LEFT) {
			motor_state = LEFT;
		}
		else if (btn_status == BTN_RIGHT) {
			motor_state = RIGHT;
		}
		motor_left(&motor_l, &motor_r);
		break;
	case RIGHT:
		if (btn_status == BTN_RUN_STOP) {
			motor_state = STOP;
		}
		else if (btn_status == BTN_LEFT) {
			motor_state = LEFT;
		}
		else if (btn_status == BTN_RIGHT) {
			motor_state = RIGHT;
		}
		motor_right(&motor_l, &motor_r);
		break;
	default:
		motor_stop(&motor_l, &motor_r);
		break;


	}
}
