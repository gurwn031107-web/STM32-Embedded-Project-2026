/*
 * dc_motor.h
 *
 *  Created on: 2026. 11. 11.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_DC_MOTOR_DC_MOTOR_H_
#define SRC_DRIVER_DC_MOTOR_DC_MOTOR_H_

#include "stm32f4xx_hal.h"

typedef struct {
	TIM_HandleTypeDef *htim;
	uint32_t Ch;
	GPIO_TypeDef* GPIO1;
	uint16_t GPIO1_Pin;
	GPIO_TypeDef* GPIO2;
	uint16_t GPIO2_Pin;
} motor_t;

void motor_init(motor_t *motor, TIM_HandleTypeDef *htim, uint32_t Ch,
		GPIO_TypeDef* GPIO1, uint16_t GPIO1_Pin,
		GPIO_TypeDef* GPIO2, uint16_t GPIO2_Pin);
void motor_set_speed(int16_t speed);
void motor_stop(motor_t *motor_left, motor_t *motor_right);
void motor_forward(motor_t *motor_left, motor_t *motor_right);
void motor_backward(motor_t *motor_left, motor_t *motor_right);
void motor_right(motor_t *motor_left, motor_t *motor_right);
void motor_left(motor_t *motor_left, motor_t *motor_right);

#endif /* SRC_DRIVER_DC_MOTOR_DC_MOTOR_H_ */
