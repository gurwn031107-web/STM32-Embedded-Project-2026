/*
 * dc_motor.c
 *
 *  Created on: 2026. 11. 11.
 *      Author: kccistc
 */

#include "dc_motor.h"

int16_t motor_speed = 0;

void motor_init(motor_t *motor, TIM_HandleTypeDef *htim, uint32_t Ch,
		GPIO_TypeDef* GPIO1, uint16_t GPIO1_Pin,
		GPIO_TypeDef* GPIO2, uint16_t GPIO2_Pin)
{
	motor->htim = htim;
	motor->Ch = Ch;
	motor->GPIO1 = GPIO1;
	motor->GPIO1_Pin = GPIO1_Pin;
	motor->GPIO2 = GPIO2;
	motor->GPIO2_Pin = GPIO2_Pin;
}

void motor_set_speed(int16_t speed)
{
	if (speed > 1000) {
		speed = 999;
	}
	else if (speed < 0) {
		speed = 0;
	}

	motor_speed = speed;
}

void motor_stop(motor_t *motor_left, motor_t *motor_right)
{
	HAL_TIM_PWM_Stop(motor_left->htim, motor_left->Ch);
	HAL_TIM_PWM_Stop(motor_right->htim, motor_right->Ch);
}

void motor_forward(motor_t *motor_left, motor_t *motor_right)
{
	__HAL_TIM_SET_COMPARE(motor_left->htim, motor_left->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_RESET);

	__HAL_TIM_SET_COMPARE(motor_right->htim, motor_right->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_SET);

	HAL_TIM_PWM_Start(motor_left->htim, motor_left->Ch);
	HAL_TIM_PWM_Start(motor_right->htim, motor_right->Ch);
}

void motor_backward(motor_t *motor_left, motor_t *motor_right)
{
	__HAL_TIM_SET_COMPARE(motor_left->htim, motor_left->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_SET);

	__HAL_TIM_SET_COMPARE(motor_right->htim, motor_right->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_RESET);

	HAL_TIM_PWM_Start(motor_left->htim, motor_left->Ch);
	HAL_TIM_PWM_Start(motor_right->htim, motor_right->Ch);
}

void motor_right(motor_t *motor_left, motor_t *motor_right)
{
	__HAL_TIM_SET_COMPARE(motor_left->htim, motor_left->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_RESET);

	__HAL_TIM_SET_COMPARE(motor_right->htim, motor_right->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_RESET);

	HAL_TIM_PWM_Start(motor_left->htim, motor_left->Ch);
	HAL_TIM_PWM_Start(motor_right->htim, motor_right->Ch);
}

void motor_left(motor_t *motor_left, motor_t *motor_right)
{
	__HAL_TIM_SET_COMPARE(motor_left->htim, motor_left->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_left->GPIO1, motor_left->GPIO1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor_left->GPIO2, motor_left->GPIO2_Pin, GPIO_PIN_RESET);

	__HAL_TIM_SET_COMPARE(motor_right->htim, motor_right->Ch, motor_speed);
	HAL_GPIO_WritePin(motor_right->GPIO1, motor_right->GPIO1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(motor_right->GPIO2, motor_right->GPIO2_Pin, GPIO_PIN_SET);

	HAL_TIM_PWM_Start(motor_left->htim, motor_left->Ch);
	HAL_TIM_PWM_Start(motor_right->htim, motor_right->Ch);
}
