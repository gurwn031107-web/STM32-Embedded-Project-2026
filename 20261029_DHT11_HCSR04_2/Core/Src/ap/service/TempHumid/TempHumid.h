/*
 * TempHumid.h
 *
 *  온습도 서비스 task
 *  - 2초마다 DHT11을 읽어서 최신 값을 보관
 *  - 응용(ap_main)은 센서를 직접 읽지 않고 TempHumid_getData()로 값만 가져감
 */

#ifndef SRC_AP_TEMPHUMID_TEMPHUMID_H_
#define SRC_AP_TEMPHUMID_TEMPHUMID_H_

#include "stm32f4xx_hal.h"
#include "../../../driver/dht11/dht11.h"

typedef struct {
	uint8_t temp_int;
	uint8_t temp_dec;
	uint8_t humid_int;
	uint8_t humid_dec;
} TempHumid_t;

void TH_init(void);
void TH_task(void);
void TH_getTempHumid(dht11_data_t *tempHumid);

#endif /* SRC_AP_TEMPHUMID_TEMPHUMID_H_ */
