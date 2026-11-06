/*
 * stepmotor.c
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */

#include "StepMotor.h"

// bit0 = IN1, bit1 = IN2, bit2 = IN3, bit3 = IN4
static const uint8_t step_table[STEP_COUNT] = {
	0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09
};

static void StepMotor_writePattern(stepMotor_t *m, uint8_t pattern)
{
	for (int i = 0; i < 4; i++) {
		GPIO_PinState s = (pattern & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
		HAL_GPIO_WritePin(m->pin->port[i], m->pin->pin[i], s);
	}
}

void StepMotor_init(stepMotor_t *m, stepMotor_pin_t *pin)
{
	m->pin = pin;
	m->stepCounter = 0;
	StepMotor_writePattern(m, 0x00);   // 시작은 코일 OFF
}

void StepMotor_stepPhase(stepMotor_t *m, stepMotor_dir_t dir)
{
	if (dir == STEP_MOTOR_DIR_CW) {
		m->stepCounter = (m->stepCounter + 1) % STEP_COUNT;               // 0 → 1 → … → 7 → 0
	}
	else {
		m->stepCounter = (m->stepCounter + STEP_COUNT - 1) % STEP_COUNT;  // 0 → 7 → 6 → …
	}
	StepMotor_writePattern(m, step_table[m->stepCounter]);
}

void StepMotor_release(stepMotor_t *m)
{
	StepMotor_writePattern(m, 0x00);
}

