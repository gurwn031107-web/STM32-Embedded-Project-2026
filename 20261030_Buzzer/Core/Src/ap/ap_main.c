/*
 * ap_main.c
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */


#include <stdio.h>
#include "ap_main.h"
#include "service/sound/sound.h"
#include "../driver/button/button.h"
#include "../driver/lcd/lcd.h"

extern I2C_HandleTypeDef hi2c1;

int ap_main(void)
{
	sound_init();
	Lcd_init(&hi2c1);

	button_t btn1, btn2, btn3;

	button_init(&btn1, GPIOC, GPIO_PIN_0);
	button_init(&btn2, GPIOC, GPIO_PIN_1);
	button_init(&btn3, GPIOC, GPIO_PIN_3);

	char buff[30];
	sprintf(buff, "Power On!");
	Lcd_writeStringXY(0, 0, buff);

	sound_power();

	uint8_t state = 0;
	uint32_t counter = 0;

	while(1)
	{
		if (button_getState(&btn1) == ACT_RELEASE) {
			sound_button();
			state = 0;
		}
		if (button_getState(&btn2) == ACT_RELEASE) {
			sound_error();
			state = 1;
		}
		if (button_getState(&btn3) == ACT_RELEASE) {
			sound_alarm();
			state = 2;
		}

		switch (state)
		{
		case 0:
			sprintf(buff, "count: %04lu", counter);
			Lcd_writeStringXY(1, 0, buff);
			break;
		case 1:
			sprintf(buff, "count: %04lu", counter++);
			Lcd_writeStringXY(1, 0, buff);
			break;
		case 2:
			counter = 0;
			sprintf(buff, "count: %04lu", counter);
			Lcd_writeStringXY(1, 0, buff);
			break;
		default:
			counter = 0;
			sprintf(buff, "count: %04lu", counter);
			Lcd_writeStringXY(1, 0, buff);
			break;

		}
		HAL_Delay(10);
	}

	return 0;
}














