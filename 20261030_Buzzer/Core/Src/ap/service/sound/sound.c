/*
 * sound.c
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */

#include "sound.h"
#include "../../../driver/buzzer/buzzer.h"

#define C4	262
#define D4	293
#define E4	330
#define F4	349
#define G4	392
#define A4	440
#define B4	494

#define C5	(C4*2)
#define D5	(D4*2)
#define E5	(E4*2)
#define F5	(F4*2)
#define G5	(G4*2)
#define A5	(A4*2)
#define B5	(B4*2)

#define C6	(C5*2)
#define D6	(D5*2)
#define E6	(E5*2)
#define F6	(F5*2)
#define G6	(G5*2)
#define A6	(A5*2)
#define B6	(B5*2)

#define C7	(C6*2)
#define D7	(D6*2)
#define E7	(E6*2)
#define F7	(F6*2)
#define G7	(G6*2)
#define A7	(A6*2)
#define B7	(B6*2)

extern TIM_HandleTypeDef htim1;

void sound_init(void)
{
	buzzer_init(&htim1, TIM_CHANNEL_4);
}

void sound_power(void)
{
	buzzer_on(C6);
	HAL_Delay(100);
	buzzer_on(E6);
	HAL_Delay(100);
	buzzer_on(G6);
	HAL_Delay(100);
	buzzer_on(C7);
	HAL_Delay(100);
	buzzer_off();
}

void sound_button(void)
{
	buzzer_on(G6);
	HAL_Delay(40);
	buzzer_on(C7);
	HAL_Delay(60);
	buzzer_off();
}

void sound_error(void)
{
	buzzer_on(A7);
	HAL_Delay(150);
	buzzer_off();
	HAL_Delay(60);
	buzzer_on(A7);
	HAL_Delay(150);
	buzzer_off();
}

void sound_alarm(void)
{
	for (int i = 0; i < 10; i++) {
		buzzer_on(A6);
		HAL_Delay(500);
		buzzer_on(E6);
		HAL_Delay(500);
	}
	buzzer_off();
}
