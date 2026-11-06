/*
 * ap_main.c
 *
 *  Created on: 2026. 10. 16.
 *      Author: kccistc
 *
 *  [변경] 2026. 10. 17. 시계 기능 추가
 *   - btn_mode(PC0) 로 시계 <-> 스톱워치 전환
 *   - TIM3 (1ms 인터럽트) 하나로
 *       1) FND 한 자리씩 켜기
 *       2) 시계 시간 증가
 *       3) 스톱워치 시간 증가 (RUN 일 때만)
 *
 *  버튼                         LED
 *   PC0 : btn_mode               PB14 : 시계 모드
 *   PC1 : btn_run_stop           PB15 : 스톱워치 모드
 *   PC3 : btn_clear              PB13 : stop  (StopWatch.c)
 *                                PC4  : run   (StopWatch.c)
 */

#include "ap_main.h"
#include "StopWatch/StopWatch.h"
#include "Watch/Watch.h"
#include "../driver/fnd/fnd.h"
#include "../driver/led/led.h"
#include "../driver/button/button.h"

typedef enum {MODE_WATCH, MODE_STOPWATCH} apMode_t;

static apMode_t mode;

static button_t btn_mode;
static led_t led_watch;
static led_t led_stopwatch;

extern TIM_HandleTypeDef htim3;

// 타이머 인터럽트가 발생하면 HAL이 이 함수를 불러줌 (TIM3 : 1ms 주기)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM3) {
		fnd_show_number();	// FND 한 자리씩 켜기
		Watch_tick();		// 시계 시간 증가 (모드와 상관없이 항상)
		StopWatch_tick();	// 스톱워치 시간 증가 (RUN 일 때만)
	}
}

int ap_main(void)
{
	mode = MODE_WATCH;

	button_init(&btn_mode, GPIOC, GPIO_PIN_0);
	led_init(&led_watch, GPIOB, GPIO_PIN_14);
	led_init(&led_stopwatch, GPIOB, GPIO_PIN_15);

	Watch_init();
	StopWatch_init();

	HAL_TIM_Base_Start_IT(&htim3);	// 1ms 타이머 인터럽트 시작

	while(1) //super loop
	{
		switch(mode)
		{
		case MODE_WATCH:
			if (button_getState(&btn_mode) == ACT_PUSH) {
				mode = MODE_STOPWATCH;
			}
			led_on(&led_watch);
			led_off(&led_stopwatch);
			Watch_task();
			break;

		case MODE_STOPWATCH:
			if (button_getState(&btn_mode) == ACT_PUSH) {
				mode = MODE_WATCH;
			}
			led_off(&led_watch);
			led_on(&led_stopwatch);
			StopWatch_task();
			break;

		default:
			mode = MODE_WATCH;
			break;
		}
	}
	return 0;
}
