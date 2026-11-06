/*

inputEvent.c*
Created on: 2026. 11. 2.
Author: kccistc*/

#include "inputEvent.h"
#include "../que/que.h"
#include "../../../driver/button/button.h"

static button_t btnMode;

void inputEvent_init(void)
{
	button_init(&btnMode, GPIOC, GPIO_PIN_0);
}

void inputEvent_task(void)
{
	static uint32_t prevTime = 0;
	if (HAL_GetTick() - prevTime < 50)
	{
		return;
	}
	prevTime = HAL_GetTick();

	if (button_getState(&btnMode) == ACT_RELEASE) {
		que_putMode(ACT_RELEASED);
	}
}
