/*
 * motor_control_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */

#include "motor_control_task.h"
#include "../../../driver/dc_motor/dc_motor.h"
#include "../../../driver/lcd/lcd.h"
#include "../../message.h" // enum 자료형을 가져다가 사용.
#include "cmsis_os.h"

typedef enum {STOP, RUN, LEFT, RIGHT} motor_state_t;

extern TIM_HandleTypeDef htim3;

static motor_t motor_l, motor_r;
static motor_state_t motor_state = STOP;
static btn_status_t btn_status = BTN_NO;
//static comm_status_t comm_status = COMM_NO;
static comm_data_t comm_data_dummy = {0};
static comm_data_t *pcomm_data;

void motor_control_task_init(void)
{
	motor_init(&motor_r, &htim3, TIM_CHANNEL_3, GPIOA, GPIO_PIN_5, GPIOA, GPIO_PIN_6);
	motor_init(&motor_l, &htim3, TIM_CHANNEL_1, GPIOB, GPIO_PIN_6, GPIOA, GPIO_PIN_7);
	motor_set_speed(900);

}

void motor_control_task(void)
{
	osEvent btn_evt;
	btn_evt = osMessageGet(btnQueHandle, 100);
			if(btn_evt.status == osEventMessage) {
				btn_status = btn_evt.value.v;
			}
			else {
				btn_status = BTN_NO;
			}

			osEvent comm_evt;
			comm_evt = osMessageGet(commQueueHandle, 100);
			if (comm_evt.status == osEventMessage) {
				pcomm_data = comm_evt.value.p;
			}
			else {
//				comm_status = COMM_NO;
				pcomm_data = &comm_data_dummy;
			}

			if (pcomm_data->status == COMM_SPEED) {
				motor_set_speed(pcomm_data->speed);
			}
			switch(motor_state)
			{
			case STOP:
				if ((btn_status == BTN_RUN_STOP) || (pcomm_data->status == COMM_RUN)) {
					motor_state = RUN;
				}
				else if (btn_status == BTN_LEFT) {
					motor_state = LEFT;
				}
				else if (btn_status == BTN_RIGHT) {
					motor_state = RIGHT;
				}
				if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
					Lcd_writeStringXY(0, 0, "Motor State STOP");
					osMutexRelease(lcdMutexHandle);
				}
				motor_stop(&motor_l, &motor_r);
				break;
			case RUN:
				if ((btn_status == BTN_RUN_STOP) || (pcomm_data->status == COMM_STOP)) {
					motor_state = STOP;
				}
				else if (btn_status == BTN_LEFT) {
					motor_state = LEFT;
				}
				else if (btn_status == BTN_RIGHT) {
					motor_state = RIGHT;
				}
				if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
					Lcd_writeStringXY(0, 0, "Motor State RUN ");
					osMutexRelease(lcdMutexHandle);
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
				if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
					Lcd_writeStringXY(0, 0, "Motor State LEFT");
					osMutexRelease(lcdMutexHandle);
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
				if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
					Lcd_writeStringXY(0, 0, "Motor State RIGHT");
					osMutexRelease(lcdMutexHandle);
				}
				motor_right(&motor_l, &motor_r);
				break;
			default:
				motor_stop(&motor_l, &motor_r);
				break;
			}
}
