/*
 * fnd.c
 *
 *  Created on: 2026. 10. 14.
 *      Author: JVO
 */

#include "fnd.h"

#define FND_DIGIT_1 	1
#define FND_DIGIT_2 	2
#define FND_DIGIT_3 	3
#define FND_DIGIT_4 	4


typedef struct {
	GPIO_TypeDef* GPIOx;
	uint16_t GPIO_Pin;
}seg_port_t;


static void fnd_all_off(void);
static void fnd_sel_digit(int digit);
static void fnd_disp_number(int number);

static void fnd_all_off(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // D1
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
}

static void fnd_sel_digit(int digit)
{
	switch(digit)
	{
	case 1:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);   // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 2 :
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 3 :
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // D1
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // D2
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);   // D3
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  // D4
		break;
	case 4 :
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // D1
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
			//    a  b  c  d  e  f  g  dp
			{1, 1, 1, 1, 1, 1, 0, 0},  // 0
			{0, 1, 1, 0, 0, 0, 0, 0},  // 1
			{1, 1, 0, 1, 1, 0, 1, 0},  // 2
			{1, 1, 1, 1, 0, 0, 1, 0},  // 3
			{0, 1, 1, 0, 0, 1, 1, 0},  // 4
			{1, 0, 1, 1, 0, 1, 1, 0},  // 5
			{1, 0, 1, 1, 1, 1, 1, 0},  // 6
			{1, 1, 1, 0, 0, 0, 0, 0},  // 7
			{1, 1, 1, 1, 1, 1, 1, 0},  // 8
			{1, 1, 1, 1, 0, 1, 1, 0},  // 9
	};
	for (int i = 0; i < 8; i++) {
		HAL_GPIO_WritePin(seg_font[i].GPIOx, seg_font[i].GPIO_Pin, seg_font_data[number][i]);
	}
}

void fnd_show_number(int number)
{
	static int state = 0;

	int data = number;

	fnd_all_off();
	switch(state)
	{
	case 0:
		fnd_all_off();
		fnd_disp_number(data / 1000 % 10);
		fnd_sel_digit(FND_DIGIT_1);
		HAL_Delay(1);
		break;
	case 1:
		fnd_all_off();
		fnd_disp_number((data / 100 % 10));
		fnd_sel_digit(FND_DIGIT_2);
		HAL_Delay(1);
		break;
	case 2:
		fnd_all_off();
		fnd_disp_number(data / 10 % 10);
		fnd_sel_digit(FND_DIGIT_3);
		HAL_Delay(1);
		break;
	case 3:
		fnd_all_off();
		fnd_disp_number(data % 10);
		fnd_sel_digit(FND_DIGIT_4);
		HAL_Delay(1);
		break;
	}
	HAL_Delay(1);
	state = ((state + 1)% 4);
}
