/*
 * Distance.c
 *
 *  초음파 거리 서비스 task
 */

#include "Distance.h"
#include <stdbool.h>

#define TRIG_PORT			GPIOC
#define TRIG_PIN			GPIO_PIN_5
#define ECHO_PORT			GPIOC
#define ECHO_PIN			GPIO_PIN_6

#define MEASURE_PERIOD_MS	100		// 데이터시트: 측정 주기 60ms 이상
#define ECHO_MIN_US			116		// 2cm   (116us / 58)
#define ECHO_MAX_US			23200	// 400cm (23200us / 58)

extern TIM_HandleTypeDef htim10;	// 1MHz(1us) 카운터

static uint32_t dist_cm = 0;
static hcsr04_status_t dist_status = HCSR04_ERR_NOT_STARTED;

void Distance_init(void)
{
	HCSR04_init(TRIG_PORT, TRIG_PIN, ECHO_PORT, ECHO_PIN, &htim10);
}

/* Echo 핀(PC6) EXTI 콜백에서 호출 */
void Distance_echoISR(void)
{
	HCSR04_echoISR();
}

void Distance_task(void)
{
	static uint32_t prevTime = 0;
	static bool measuring = false;

	// 1. 측정 중이면 결과가 나왔는지 확인 (기다리지 않고 바로 리턴)
	if (measuring) {
		uint32_t echo_us;
		hcsr04_status_t s = HCSR04_getResult(&echo_us);

		if (s == HCSR04_IN_PROGRESS) {
			return;
		}
		measuring = false;

		if (s == HCSR04_OK) {
			if (echo_us < ECHO_MIN_US || echo_us > ECHO_MAX_US) {
				s = HCSR04_ERR_OUT_OF_RANGE;
			}
			else {
				dist_cm = echo_us * 17 / 1000;
			}
		}
		dist_status = s;
		return;
	}

	// 2. 측정 주기마다 Trig 펄스 발사
	if (HAL_GetTick() - prevTime < MEASURE_PERIOD_MS) {
		return;
	}
	prevTime = HAL_GetTick();

	if (HCSR04_start() == HCSR04_OK) {
		measuring = true;
	}
}

hcsr04_status_t Distance_get(uint32_t *distance_cm)
{
	*distance_cm = dist_cm;
	return dist_status;
}
