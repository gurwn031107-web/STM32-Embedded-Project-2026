/*
 * fnd.c
 *
 *  Created on: 2026. 10. 14.
 *      Author: kccistc
 */

#include "fnd.h"

typedef struct seg_port {
	GPIO_TypeDef* GPIOx;
	uint16_t GPIO_Pin;
} seg_port_t;

static void fnd_all_off(void);
static void fnd_sel_digit(int digit);
static void fnd_disp_number(int number);
static void fnd_disp_dp(int digit);

static volatile uint16_t num = 0;
static volatile uint16_t dp_data = 0;	// 자리별 dp 상태 (bit0 = 1번째 자리 ... bit3 = 4번째 자리)

void fnd_set_num(uint16_t n)
{
    num = n;
}

uint16_t fnd_get_num(void)
{
	return num;
}

// dig 자리의 dp(점)를 켜거나 끔 : fnd_set_dp(FND_DIGIT_2, FND_DP_ON)
void fnd_set_dp(int dig, uint16_t state)
{
	if (state == FND_DP_ON) {
		dp_data |= (1 << dig);
	}
	else {
		dp_data &= ~(1 << dig);
	}
}

uint16_t fnd_get_dp(void)
{
	return dp_data;
}

static void fnd_disp_dp(int digit)
{
	if (fnd_get_dp() & (1 << digit)) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	}
	else {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
	}
}

static void fnd_all_off(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); //D1
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); //D2
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET); //D3
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); //D4
}

static void fnd_sel_digit(int digit)
{
	switch (digit)
	{
	case 0:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 1:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 2:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 3:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);  // D4
		break;
	}
}

static void fnd_disp_number(int number)
{
	seg_port_t seg_font[8] = {
			{GPIOA, GPIO_PIN_6},
			{GPIOA, GPIO_PIN_9},
			{GPIOB, GPIO_PIN_5},
			{GPIOB, GPIO_PIN_10},
			{GPIOA, GPIO_PIN_8},
			{GPIOA, GPIO_PIN_7},
			{GPIOB, GPIO_PIN_3},
			{GPIOB, GPIO_PIN_4}
	};

	int seg_font_data[10][8] = {
			{1, 1, 1, 1, 1, 1, 0, 0}, // 0
			{0, 1, 1, 0, 0, 0, 0, 0}, // 1
			{1, 1, 0, 1, 1, 0, 1, 0}, // 2
			{1, 1, 1, 1, 0, 0, 1, 0}, // 3
			{0, 1, 1, 0, 0, 1, 1, 0}, // 4
			{1, 0, 1, 1, 0, 1, 1, 0}, // 5
			{1, 0, 1, 1, 1, 1, 1, 0}, // 6
			{1, 1, 1, 0, 0, 0, 0, 0}, // 7
			{1, 1, 1, 1, 1, 1, 1, 0}, // 8
			{1, 1, 1, 1, 0, 1, 1, 0}, // 9
	};

	for (int i=0; i<8; i++) {
		HAL_GPIO_WritePin(seg_font[i].GPIOx, seg_font[i].GPIO_Pin, seg_font_data[number][i]);

	}
}

void fnd_show_number(void)
{
	static int state = 0;

	int data = fnd_get_num();

	fnd_all_off();
	switch(state)
	{
	case 0:
		fnd_disp_number(data / 1000 % 10);
		fnd_disp_dp(FND_DIGIT_1);
		fnd_sel_digit(FND_DIGIT_1);
		break;

	case 1:
		fnd_disp_number(data / 100 % 10);
		fnd_disp_dp(FND_DIGIT_2);
		fnd_sel_digit(FND_DIGIT_2);
		break;

	case 2:
		fnd_disp_number(data / 10 % 10);
		fnd_disp_dp(FND_DIGIT_3);
		fnd_sel_digit(FND_DIGIT_3);
		break;

	case 3:
		fnd_disp_number(data % 10);
		fnd_disp_dp(FND_DIGIT_4);
		fnd_sel_digit(FND_DIGIT_4);
		break;
	default:
		break;
	}
//	HAL_Delay(1);
	state = (state + 1) % 4;
}
