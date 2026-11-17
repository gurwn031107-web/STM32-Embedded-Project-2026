/*
 * ultrasonic.c
 *
 *  Created on: 2026. 11. 12.
 *
 *  HC-SR04 초음파 센서 드라이버
 *
 *  us(마이크로초) 시간 측정은 Cortex-M4 안에 있는 DWT 사이클 카운터를 쓴다.
 *   - DWT->CYCCNT : CPU 클럭(100MHz)마다 1씩 증가하는 32비트 카운터
 *   - 100 사이클 = 1us
 *   - CubeMX에서 타이머를 따로 설정하지 않아도 된다.
 */

#include "ultrasonic.h"

#define ECHO_BUSY_TIMEOUT_US    1000   // 측정 전 ECHO가 Low로 내려오기까지 최대 대기
#define ECHO_START_TIMEOUT_US   6000   // TRIG 후 ECHO가 High로 올라오기까지 최대 대기
                                       // (참고 코드와 같은 6ms. 일부 호환 센서는 늦게 올라옴)
#define ECHO_HIGH_TIMEOUT_US    12000  // ECHO High 최대 폭 (약 2m 거리)

/* ---------------- us 단위 시간 함수 (DWT) ---------------- */
static void dwt_init(void)
{
	static uint8_t ready = 0;
	if (ready) return;

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // DWT 사용 허가
	DWT->CYCCNT = 0;                                // 카운터 0으로
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // 카운터 시작
	ready = 1;
}

// start 시점부터 지난 시간[us]
// (unsigned 뺄셈이라 카운터가 한 바퀴 돌아 0이 되어도 올바르게 계산됨)
static uint32_t dwt_elapsed_us(uint32_t start)
{
	return (DWT->CYCCNT - start) / (SystemCoreClock / 1000000);
}

static void delay_us(uint32_t us)
{
	uint32_t start = DWT->CYCCNT;
	while (dwt_elapsed_us(start) < us);
}

/* ---------------- 드라이버 함수 ---------------- */
void ultrasonic_init(ultrasonic_t *us,
		GPIO_TypeDef* TRIG, uint16_t TRIG_Pin,
		GPIO_TypeDef* ECHO, uint16_t ECHO_Pin)
{
	us->TRIG = TRIG;
	us->TRIG_Pin = TRIG_Pin;
	us->ECHO = ECHO;
	us->ECHO_Pin = ECHO_Pin;

	dwt_init();
	HAL_GPIO_WritePin(us->TRIG, us->TRIG_Pin, GPIO_PIN_RESET);
}

uint16_t ultrasonic_getDistance(ultrasonic_t *us)
{
	uint32_t start;
	uint32_t echo_us;

	// 0) 측정 전에 ECHO가 Low인지 확인
	//    High로 계속 있으면 이전 측정이 안 끝났거나 센서가 멈춘(고착) 상태
	start = DWT->CYCCNT;
	while (HAL_GPIO_ReadPin(us->ECHO, us->ECHO_Pin) == GPIO_PIN_SET) {
		if (dwt_elapsed_us(start) > ECHO_BUSY_TIMEOUT_US) {
			return ULTRASONIC_ERR_BUSY;          // E3
		}
	}

	// 1) TRIG 10us 펄스 출력
	HAL_GPIO_WritePin(us->TRIG, us->TRIG_Pin, GPIO_PIN_RESET);
	delay_us(2);
	HAL_GPIO_WritePin(us->TRIG, us->TRIG_Pin, GPIO_PIN_SET);
	delay_us(10);
	HAL_GPIO_WritePin(us->TRIG, us->TRIG_Pin, GPIO_PIN_RESET);

	// 2) ECHO가 High가 될 때까지 대기 (선이 빠졌으면 시간 초과)
	start = DWT->CYCCNT;
	while (HAL_GPIO_ReadPin(us->ECHO, us->ECHO_Pin) == GPIO_PIN_RESET) {
		if (dwt_elapsed_us(start) > ECHO_START_TIMEOUT_US) {
			return ULTRASONIC_ERR_NO_START;      // E1
		}
	}

	// 3) ECHO가 High인 시간 측정
	start = DWT->CYCCNT;
	while (HAL_GPIO_ReadPin(us->ECHO, us->ECHO_Pin) == GPIO_PIN_SET) {
		if (dwt_elapsed_us(start) > ECHO_HIGH_TIMEOUT_US) {
			return ULTRASONIC_ERR_TOO_LONG;      // E2: 너무 멀거나 반사파 없음
		}
	}
	echo_us = dwt_elapsed_us(start);

	// 4) 거리 계산: echo_us x 0.034 / 2  ->  정수 계산: echo_us x 34 / 2000
	return (uint16_t)((echo_us * 34) / 2000);
}
