/*
 * ultrasonic.h
 *
 *  Created on: 2026. 11. 12.
 *
 *  HC-SR04 초음파 센서 드라이버
 *  (참고: 3-Way-RC-Car/RC_Car ultrasonic.c + delay.c 를
 *   button_t, motor_t 처럼 "구조체 + init" 스타일로 정리)
 *
 *  동작 원리
 *   1) TRIG 핀에 10us 동안 High 펄스를 준다.
 *   2) 센서가 초음파를 쏘고, 반사파가 돌아올 때까지 ECHO 핀이 High가 된다.
 *   3) 거리[cm] = ECHO High 시간[us] x 0.034[cm/us] / 2(왕복)
 */

#ifndef SRC_DRIVER_ULTRASONIC_ULTRASONIC_H_
#define SRC_DRIVER_ULTRASONIC_ULTRASONIC_H_

#include "stm32f4xx_hal.h"

/* 측정 실패 시 반환값 (원인별로 구분해서 LCD에 E1/E2/E3로 표시) */
#define ULTRASONIC_ERR_NO_START  901  // E1: Trig 후 ECHO가 High로 안 올라옴
                                      //     → 배선(Trig/Echo/VCC/GND), 센서 불량
#define ULTRASONIC_ERR_TOO_LONG  902  // E2: ECHO High가 너무 김
                                      //     → 2m 안에 물체 없음, 또는 센서를 너무 가까이 막음
#define ULTRASONIC_ERR_BUSY      903  // E3: 측정 시작 전부터 ECHO가 High
                                      //     → 센서가 High로 멈춤(고착), 센서 전원 껐다 켜기

typedef struct {
	GPIO_TypeDef* TRIG;
	uint16_t TRIG_Pin;
	GPIO_TypeDef* ECHO;
	uint16_t ECHO_Pin;
} ultrasonic_t;

void ultrasonic_init(ultrasonic_t *us,
		GPIO_TypeDef* TRIG, uint16_t TRIG_Pin,
		GPIO_TypeDef* ECHO, uint16_t ECHO_Pin);
uint16_t ultrasonic_getDistance(ultrasonic_t *us); // [cm], 실패 시 ULTRASONIC_ERR_xxx (900번대)

#endif /* SRC_DRIVER_ULTRASONIC_ULTRASONIC_H_ */
