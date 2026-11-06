/*
 * TempHumid.c
 *
 */


#include "TempHumid.h"

#define dht_port	GPIOC
#define dht_pin		GPIO_PIN_8

static dht11_data_t TH_data;

void TH_init(void)
{
	dht11_init(dht_port, dht_pin);
}

void TH_task(void)
{
	static uint32_t prevTime = 0;
	if (HAL_GetTick() - prevTime < 2000) {
		return;
	}
	prevTime = HAL_GetTick();

	dht11_read(&TH_data);
}

void TH_getTempHumid(dht11_data_t *tempHumid)
{
	*tempHumid = TH_data;
}
