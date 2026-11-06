/*
 * stepmotor.h
 *
 *  Created on: 2026. 11. 5.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_
#define SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_

#include "main.h"

#define STEP_COUNT  8   // 하프스텝 패턴 수

typedef enum {
	STEP_MOTOR_DIR_CW,
	STEP_MOTOR_DIR_CCW
} stepMotor_dir_t;

typedef struct {
	GPIO_TypeDef *port[4];   // IN1 ~ IN4
	uint16_t      pin[4];
} stepMotor_pin_t;           // 모터 1개의 핀 묶음 (하드웨어)

typedef struct {
	stepMotor_pin_t *pin;    // 핀 묶음 주소
	uint8_t stepCounter;     // 현재 여자 패턴 0 ~ 7 (상태)
} stepMotor_t;               // 모터 1개

void StepMotor_init(stepMotor_t *m, stepMotor_pin_t *pin);
void StepMotor_stepPhase(stepMotor_t *m, stepMotor_dir_t dir);   // 1스텝 이동
void StepMotor_release(stepMotor_t *m);                          // 코일 전부 끄기

#endif /* SRC_DRIVER_STEPMOTOR_STEPMOTOR_H_ */
