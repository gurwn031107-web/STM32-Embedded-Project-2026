/*
 * HCSR04.h
 *
 *  Created on: 2026. 10. 28.
 *      Author: kccistc
 */

#ifndef HCSR04_H_
#define HCSR04_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	HCSR04_OK = 0,
	HCSR04_IN_PROGRESS,
	HCSR04_ERR_NOT_STARTED,
	HCSR04_ERR_BUSY,
	HCSR04_ERR_NO_ECHO,
	HCSR04_ERR_OUT_OF_RANGE
} hcsr04_status_t;

void HCSR04_init(GPIO_TypeDef *t_port, uint16_t t_pin,
		GPIO_TypeDef *e_port, uint16_t e_pin,
		TIM_HandleTypeDef *htim);
hcsr04_status_t HCSR04_start(void);
void HCSR04_echoISR(void);
hcsr04_status_t HCSR04_getResult(uint32_t *echo_us);

#endif /* HCSR04_H_ */
