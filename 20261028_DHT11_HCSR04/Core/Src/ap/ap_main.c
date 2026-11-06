/*
 * ap_main.c
 *
 *  응용 S/W: LCD에 초음파 거리(1줄), 온도/습도(2줄)를 출력
 */


#include "ap_main.h"
#include "service/TempHumid/TempHumid.h"
#include "service/Distance/Distance.h"
#include "service/display/display.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_6) {		// PC6: HC-SR04 Echo
		Distance_echoISR();
	}
}

int ap_main(void)
{
	TH_init();
	Distance_init();
	display_init();

	while(1)
	{
		TH_task();
		Distance_task();
		display_task();
	}

	return 0;
}
