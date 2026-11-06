/*
 * HCSR04.c
 *
 *  Created on: 2026. 10. 28.
 *      Author: kccistc
 */

#include "HCSR04.h"
#include "../../common/delay.h"

#define RISE_TIMEOUT_MS		5
#define FALL_TIMEOUT_MS		50

typedef enum {
	STATE_IDLE,
	STATE_WAIT_RISE,
	STATE_WAIT_FALL,
	STATE_DONE
} sr04_state_t;

static GPIO_TypeDef *trig_port;
static uint16_t trig_pin;
static GPIO_TypeDef *echo_port;
static uint16_t echo_pin;

static TIM_HandleTypeDef *sr04_htim;

static volatile sr04_state_t state;
static volatile uint32_t rise_cnt;
static volatile uint32_t fall_cnt;

static uint32_t start_ms;

static bool echo_is_high(void);
static uint32_t timer_diff(uint32_t start, uint32_t end);

void HCSR04_init(GPIO_TypeDef *t_port, uint16_t t_pin,
		GPIO_TypeDef *e_port, uint16_t e_pin,
		TIM_HandleTypeDef *htim)
{
	trig_port	= t_port;
	trig_pin	= t_pin;
	echo_port	= e_port;
	echo_pin	= e_pin;
	sr04_htim	= htim;

	delay_init();
	HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);
	HAL_TIM_Base_Start(sr04_htim);

	state = STATE_IDLE;
}

hcsr04_status_t HCSR04_start(void)
{
	if (state == STATE_WAIT_RISE || state == STATE_WAIT_FALL) {
		return HCSR04_ERR_BUSY;
	}
	if (echo_is_high()) {
		return HCSR04_ERR_BUSY;
	}

	start_ms = HAL_GetTick();
	state = STATE_WAIT_RISE;

	HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_SET);
	delay_us(10);
	HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);

	return HCSR04_OK;
}

static bool echo_is_high(void)
{
	return (HAL_GPIO_ReadPin(echo_port, echo_pin) == GPIO_PIN_SET);
}

void HCSR04_echoISR(void)
{
	// 측정 중이 아닐 때(init 전 포함) 들어온 엣지는 무시
	if (state != STATE_WAIT_RISE && state != STATE_WAIT_FALL) {
		return;
	}

	uint32_t cnt = __HAL_TIM_GET_COUNTER(sr04_htim);
	bool high = echo_is_high();

	if ((state == STATE_WAIT_RISE) && high) {
		rise_cnt = cnt;
		state = STATE_WAIT_FALL;
	}
	else if ((state == STATE_WAIT_FALL) && !high) {
		fall_cnt = cnt;
		state = STATE_DONE;
	}
}

static uint32_t timer_diff(uint32_t start, uint32_t end)
{
	uint32_t arr = __HAL_TIM_GET_AUTORELOAD(sr04_htim);

	if (end >= start) {
		return end - start;
	}
	else {
		return (arr - start) + end + 1;
	}
}

hcsr04_status_t HCSR04_getResult(uint32_t *echo_us)
{
	uint32_t elapsed_ms = HAL_GetTick() - start_ms;

	switch (state)
	{
	case STATE_WAIT_RISE:
		if (elapsed_ms <= RISE_TIMEOUT_MS) {
			return HCSR04_IN_PROGRESS;
		}
		state = STATE_IDLE;
		return HCSR04_ERR_NO_ECHO;

	case STATE_WAIT_FALL:
		if (elapsed_ms <= FALL_TIMEOUT_MS) {
			return HCSR04_IN_PROGRESS;
		}
		state = STATE_IDLE;
		return HCSR04_ERR_NO_ECHO;

	case STATE_DONE:
	{
		uint32_t us = timer_diff(rise_cnt, fall_cnt);
		state = STATE_IDLE;
		*echo_us = us;
		return HCSR04_OK;
	}

	case STATE_IDLE:
	default:
		return HCSR04_ERR_NOT_STARTED;
	}
}
