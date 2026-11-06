/*
 * ap_main.c
 *
 *  응용 S/W: LCD에 온도, 습도를 2초 간격으로 출력
 */


#include "ap_main.h"
#include "service/TempHumid/TempHumid.h"
#include "service/display/display.h"


int ap_main(void)
{
	TH_init();
	display_init();

	while(1)
	{
		TH_task();
		display_task();
	}

	return 0;
}
