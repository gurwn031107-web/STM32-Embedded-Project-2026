/*
 * ap_main.c
 *
 *  응용 S/W: 버튼 2개로 부저 PWM 제어
 *  - 버튼1: 주파수 100Hz씩 증가
 *  - 버튼2: duty cycle 10%씩 증가
 *  (이전 거리/온습도 버전은 ap_main.c.bak에 보관)
 */

#include "ap_main.h"
#include "../driver/button/button.h"
#include "../driver/buzzer/buzzer.h"
#include <stdio.h>

#define BTN_FREQ_PORT	GPIOC
#define BTN_FREQ_PIN	GPIO_PIN_0		// 버튼1: 주파수 증가
#define BTN_DUTY_PORT	GPIOC
#define BTN_DUTY_PIN	GPIO_PIN_1		// 버튼2: duty 증가

extern TIM_HandleTypeDef htim1;			// PA11 = TIM1_CH4

static void print_state(void)
{
	printf("freq %5lu Hz, duty %2lu %%\r\n", buzzer_getFreq(), buzzer_getDuty());
}

int ap_main(void)
{
	button_t btn_freq;
	button_t btn_duty;

	button_init(&btn_freq, BTN_FREQ_PORT, BTN_FREQ_PIN);
	button_init(&btn_duty, BTN_DUTY_PORT, BTN_DUTY_PIN);
	buzzer_init(&htim1, TIM_CHANNEL_4);
	print_state();

	while (1)
	{
		if (button_getState(&btn_freq) == ACT_RELEASE) {
			buzzer_freqUp();
			print_state();
		}

		if (button_getState(&btn_duty) == ACT_RELEASE) {
			buzzer_dutyUp();
			print_state();
		}
	}

	return 0;
}
