/*
 * ap_main.c
 *
 *  Created on: 2026. 10. 16.
 *      Author: kccistc
 */

#include "ap_main.h"
#include "StopWatch/StopWatch.h"
#include "TimeClock/TimeClock.h"
#include "../driver/fnd/fnd.h"
#include "../driver/button/button.h"
#include "../driver/led/led.h"

enum {TIME_CLOCK_MODE, STOP_WATCH_MODE};

button_t btnMode;
led_t ledTimeClock, ledStopWatch;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3) {
		fnd_show_number();
	}
	else if (htim->Instance == TIM4) {
		TimeClock_ISR();
	}
}

int ap_main(void)
{
	StopWatch_init();
	TimeClock_init();
	button_init(&btnMode, GPIOC, GPIO_PIN_0);
	led_init(&ledTimeClock, GPIOB, GPIO_PIN_15);
	led_init(&ledStopWatch, GPIOB, GPIO_PIN_14);

	int modeState = TIME_CLOCK_MODE;

	while(1) // super loop
	{
		switch (modeState)
		{
		case TIME_CLOCK_MODE:
			if (button_getState(&btnMode) == ACT_RELEASE) {
				modeState = STOP_WATCH_MODE;
			}
			TimeClock_task();
			led_on(&ledTimeClock);
			led_off(&ledStopWatch);
			break;

		case STOP_WATCH_MODE:
			if (button_getState(&btnMode) == ACT_RELEASE) {
				modeState = TIME_CLOCK_MODE;
			}
			StopWatch_task();
			led_off(&ledTimeClock);
			led_on(&ledStopWatch);
			break;

		default:
			break;
		}
	} return 0;
}
