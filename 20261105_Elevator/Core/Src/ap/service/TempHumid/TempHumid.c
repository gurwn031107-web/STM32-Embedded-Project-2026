/*
 * TempHumid.c
 *  (20261028_DHT11_Temp_Humid 프로젝트에서 가져옴)
 *  DHT11 DATA = PC8 (CN10-2)
 */

#include "TempHumid.h"

#define dht_port	GPIOC
#define dht_pin		GPIO_PIN_8

static dht11_data_t TH_data;
static uint8_t      TH_valid = 0;     // 한 번이라도 제대로 읽었으면 1

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

	/* 실패(응답 없음 · 체크섬)면 이전 값 유지 */
	if (dht11_read(&TH_data) == DHT11_OK) {
		TH_valid = 1;
	}
}

void TH_getTempHumid(dht11_data_t *tempHumid)
{
	*tempHumid = TH_data;
}

uint8_t TH_isValid(void)
{
	return TH_valid;
}
