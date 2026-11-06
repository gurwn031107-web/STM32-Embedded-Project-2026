/*
 * buzzer.c
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */

#include "buzzer.h"

static TIM_HandleTypeDef *buzzer_htim;
static uint32_t buzzer_channel;

void buzzer_init(TIM_HandleTypeDef *htim, uint32_t ch)
{
	buzzer_htim = htim;
	buzzer_channel = ch;

	__HAL_TIM_SET_COMPARE(buzzer_htim, buzzer_channel, 0);	// 초기값 pwm low(0V) 출력
	HAL_TIM_PWM_Start(buzzer_htim, buzzer_channel);
}

void buzzer_off(void)
{
	__HAL_TIM_SET_COMPARE(buzzer_htim, buzzer_channel, 0);
}

void buzzer_on(uint32_t freq_hz)
{
	if (freq_hz == 0) {
		buzzer_off();
		return;
	}
	if (freq_hz < 20) freq_hz = 20;
	if (freq_hz >20000) freq_hz = 20000;

	uint32_t arr = 1000000 / freq_hz - 1;
	uint32_t ccr = (arr + 1) / 2 - 1;
	__HAL_TIM_SET_AUTORELOAD(buzzer_htim, arr);
	__HAL_TIM_SET_COMPARE(buzzer_htim, buzzer_channel, ccr);
}
