/*
 * StopWatch.c
 *
 *  Created on: 2026. 10. 16.
 *      Author: kccistc
 */

#include "StopWatch.h"
#include "../../driver/button/button.h"
#include "../../driver/led/led.h"
#include "../../driver/fnd/fnd.h"

typedef enum {STOP, RUN, CLEAR} stopWatch_t;

static void StopWatch_stop();
static void StopWatch_run();
static void StopWatch_clear();
static void StopWatch_setState(stopWatch_t s);
static stopWatch_t StopWatch_getState(void);
static void StopWatch_incCounter(void);
static void StopWatch_clearCounter(void);
static uint16_t StopWatch_getCounter(void);


static volatile stopWatch_t stopWatch_state;
static volatile uint16_t counter;
static volatile uint16_t msec;

static led_t led_run;
static led_t led_stop;

static button_t btn_run_stop;
static button_t btn_clear;

static void StopWatch_setState(stopWatch_t s)
{
	stopWatch_state = s;
}

static stopWatch_t StopWatch_getState(void)
{
	return stopWatch_state;
}

static void StopWatch_incCounter(void)
{
	counter++;
}

static void StopWatch_clearCounter(void)
{
	counter = 0;
	msec = 0;
}

static uint16_t StopWatch_getCounter(void)
{
	return counter;
}


void StopWatch_init(void)
{
	stopWatch_state = STOP;
	counter = 0;
	msec = 0;

	led_init(&led_stop, GPIOC, GPIO_PIN_4);
	led_init(&led_run, GPIOB, GPIO_PIN_13);

	button_init(&btn_run_stop, GPIOC, GPIO_PIN_1);
	button_init(&btn_clear, GPIOC, GPIO_PIN_3);

	led_on(&led_stop);
	led_off(&led_run);

}

void StopWatch_tick(void)
{
	if (StopWatch_getState() != RUN) {
		return;
	}

	msec++;
	if (msec >= 100) {
		msec = 0;
		if (StopWatch_getCounter() >= 9999) {
			counter = 0;
		}
		else {
			StopWatch_incCounter();
		}
	}
}


void StopWatch_task(void)
{
	switch(stopWatch_state)
	{
	case STOP:
		StopWatch_stop();
		break;
	case RUN:
		StopWatch_run();
		break;
	case CLEAR:
		StopWatch_clear();
		break;
	default:
		StopWatch_stop();
		break;
	}

	fnd_set_num(StopWatch_getCounter());
	fnd_set_dp(FND_DP_3);
}


static void StopWatch_stop()
{
	if (button_getState(&btn_run_stop) == ACT_PUSH){
		StopWatch_setState(RUN);
	}
	else if(button_getState(&btn_clear) == ACT_RELEASE){
		StopWatch_setState(CLEAR);
	}

	led_on(&led_stop);
	led_off(&led_run);
}

static void StopWatch_run()
{
	if (button_getState(&btn_run_stop) == ACT_PUSH) {
		StopWatch_setState(STOP);
	}

	led_off(&led_stop);
	led_on(&led_run);

	// 시간 증가는 StopWatch_tick() (TIM3 인터럽트) 에서 함
}

static void StopWatch_clear()
{
	StopWatch_clearCounter();
	StopWatch_setState(STOP);
}
