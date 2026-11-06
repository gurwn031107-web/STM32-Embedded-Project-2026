/*
 * ap_main.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#include "ap_main.h"
#include "service/timeClock/timeClock.h"
#include "service/tempHumid/tempHumid.h"
#include "service/distance/distance.h"
#include "../driver/button/button.h"
#include "service/stopWatch/stopWatch.h"
#include "service/que/que.h"
#include "service/inputEvent/inputEvent.h"

typedef enum {
	S_CALENDAR = 0,
	S_TEMP_HUMID,
	S_DISTANCE,
	S_STOPWATCH
} smartWatch_state_t;

smartWatch_state_t smartWatch_state = S_CALENDAR;

button_t btnMode;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_6) {		// PC6: HC-SR04 Echo
		distance_ISR();
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3) {
		stopWatch_ISR();
	}
}

int ap_main(void)
{
	timeClock_init();
	tempHumid_init();
	distance_init();
	stopWatch_init();
	que_init();
	inputEvent_init();

	//button_init(&btnMode, GPIOC, GPIO_PIN_0);
	event_btn_act_t btn_mode = ACT_PUSHED;

	while(1)
	{
		timeClock_task(); // 계속 돌고있는 시계
		inputEvent_task();

		switch(smartWatch_state)
		{
		case S_CALENDAR: // 달력모드
			if (que_isEmptyMode() == false) {
				que_getMode(&btn_mode);
				if (btn_mode == ACT_RELEASED) {
					smartWatch_state = S_TEMP_HUMID;
				}
			}
			timeClock_calendar_task();
			break;
		case S_TEMP_HUMID: //온습도모드
			if (que_isEmptyMode() == false) {
				que_getMode(&btn_mode);
				if (btn_mode == ACT_RELEASED) {
					smartWatch_state = S_DISTANCE;
					distance_disp(0);
				}
			}
			tempHumid_task();
			break;
		case S_DISTANCE: //거리측정모드
			if (que_isEmptyMode() == false) {
				que_getMode(&btn_mode);
				if (btn_mode == ACT_RELEASED) {
					smartWatch_state = S_STOPWATCH;
					stopWatch_disp();
				}
			}
			distance_task();
			break;
		case S_STOPWATCH: //스탑워치모드
			if (que_isEmptyMode() == false) {
				que_getMode(&btn_mode);
				if (btn_mode == ACT_RELEASED) {
					smartWatch_state = S_CALENDAR;
					timeClock_setDateFirstFlag();
				}
			}
			stopWatch_task();
			break;
		default:
			break;
		}
	}
	return 0;
}

