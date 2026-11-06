/*
 * StepMotor.c
 *
 *  Created on: 2026. 10. 23.
 *      Author: kccistc
 */

#include "StepMotor.h"

#define STEP_COUNT 8

static uint8_t step_table[8] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0c, 0x08, 0x09};


static void StepMotor_writePattern(stepMotor_t *m, uint8_t step)
{
	for (int i=0; i<4; i++)
	{
		GPIO_PinState s = (step & (1<<i)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
		HAL_GPIO_WritePin(m->pin->port[i], m->pin->pin[i], s);
	}
}

void StepMotor_stepPhase(stepMotor_t *m, stepMotor_dir_t dir)
{
	if (dir == STEP_MOTOR_DIR_CW){
		m->stepCounter = (m->stepCounter  + STEP_COUNT - 1) % STEP_COUNT;
	}
	else {
		m->stepCounter = (m->stepCounter + 1) % STEP_COUNT;
	}
	StepMotor_writePattern(m, step_table[m->stepCounter]);
}

void StepMotor_release(stepMotor_t *m)
{
	StepMotor_writePattern(m, 0x00);
}


void StepMotor_hold(stepMotor_t *m)
{
    StepMotor_writePattern(m, step_table[m->stepCounter]);
}
