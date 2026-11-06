/*
 * buzzer.h
 *
 *  PWM 부저 드라이버
 */

#ifndef SRC_DRIVER_BUZZER_BUZZER_H_
#define SRC_DRIVER_BUZZER_BUZZER_H_

#include "stm32f4xx_hal.h"

void buzzer_init(TIM_HandleTypeDef *htim, uint32_t channel);
void buzzer_freqUp(void);
void buzzer_dutyUp(void);
uint32_t buzzer_getFreq(void);
uint32_t buzzer_getDuty(void);

#endif /* SRC_DRIVER_BUZZER_BUZZER_H_ */
