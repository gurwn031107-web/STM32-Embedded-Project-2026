/*
 * buzzer.c
 *
 *  PWM 부저 드라이버
 *  - 타이머 카운터 1MHz (100MHz / Prescaler 100) 기준
 *  - 주파수: ARR + 1 = 1,000,000 / freq
 *  - 듀티비: CCR     = (ARR + 1) * duty / 100
 */

#include "buzzer.h"

#define TIMER_CNT_HZ	1000000		// 100MHz / (99 + 1)

#define FREQ_MIN		100			// Hz
#define FREQ_MAX		20000		// Hz (가청 주파수 상한)
#define FREQ_STEP		100			// 버튼 1번 누를 때마다 +100Hz

#define DUTY_MIN		0			// %
#define DUTY_MAX		90			// %
#define DUTY_STEP		10			// 버튼 1번 누를 때마다 +10%

static TIM_HandleTypeDef *buz_htim;
static uint32_t buz_ch;

static uint32_t freq = 1000;		// 시작: 1kHz
static uint32_t duty = 50;			// 시작: 50%

/* 현재 freq, duty 값으로 ARR(주기)과 CCR(High 길이)을 다시 계산 */
static void buzzer_apply(void)
{
	uint32_t period = TIMER_CNT_HZ / freq;		// ARR + 1

	__HAL_TIM_SET_AUTORELOAD(buz_htim, period - 1);
	// 주파수만 바뀌어도 CCR을 다시 계산해야 듀티비가 유지된다
	__HAL_TIM_SET_COMPARE(buz_htim, buz_ch, period * duty / 100);
}

void buzzer_init(TIM_HandleTypeDef *htim, uint32_t channel)
{
	buz_htim = htim;
	buz_ch = channel;

	buzzer_apply();
	// preload된 ARR/CCR 값을 바로 적용 (첫 주기부터 1kHz로 시작)
	HAL_TIM_GenerateEvent(buz_htim, TIM_EVENTSOURCE_UPDATE);
	HAL_TIM_PWM_Start(buz_htim, buz_ch);
}

/* 주파수 증가 함수: 100Hz씩 증가, 20kHz를 넘으면 100Hz로 */
void buzzer_freqUp(void)
{
	freq += FREQ_STEP;
	if (freq > FREQ_MAX) {
		freq = FREQ_MIN;
	}
	buzzer_apply();
}

/* duty cycle 증가 함수: 10%씩 증가, 90%를 넘으면 0%로 */
void buzzer_dutyUp(void)
{
	duty += DUTY_STEP;
	if (duty > DUTY_MAX) {
		duty = DUTY_MIN;
	}
	buzzer_apply();
}

uint32_t buzzer_getFreq(void)
{
	return freq;
}

uint32_t buzzer_getDuty(void)
{
	return duty;
}
