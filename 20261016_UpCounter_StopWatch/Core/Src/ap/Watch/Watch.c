/*
 * Watch.c
 *
 *  Created on: 2026. 10. 17.
 *
*/

#include "Watch.h"
#include "../../driver/fnd/fnd.h"


static volatile uint16_t msec;	// 0 ~ 999
static volatile uint8_t  sec;	// 0 ~ 59
static volatile uint8_t  min;	// 0 ~ 59

void Watch_init(void)
{
	msec = 0;
	sec  = 0;
	min  = 0;
}

void Watch_tick(void)
{
	msec++;
	if (msec >= 1000) {
		msec = 0;
		sec++;
		if (sec >= 60) {
			sec = 0;
			min++;
			if (min >= 60) {
				min = 0;
			}
		}
	}
}

void Watch_task(void)
{
	fnd_set_num(min * 100 + sec);

	if (msec < 500) {
		fnd_set_dp(FND_DP_2);
	}
	else {
		fnd_set_dp(FND_DP_NONE);
	}
}
