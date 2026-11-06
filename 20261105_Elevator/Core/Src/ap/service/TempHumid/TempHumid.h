/*
 * TempHumid.h
 *
 *  온습도 서비스 task  (20261028_DHT11_Temp_Humid 프로젝트에서 가져옴)
 *  - 2초마다 DHT11을 읽어서 최신 값을 보관
 *  - 응용(ap_main)은 센서를 직접 읽지 않고 TH_getTempHumid()로 값만 가져감
 *  - 엘리베이터용 추가: TH_isValid() → 한 번이라도 제대로 읽었는지
 */

#ifndef SRC_AP_TEMPHUMID_TEMPHUMID_H_
#define SRC_AP_TEMPHUMID_TEMPHUMID_H_

#include "stm32f4xx_hal.h"
#include "../../../driver/dht11/dht11.h"

void    TH_init(void);
void    TH_task(void);
void    TH_getTempHumid(dht11_data_t *tempHumid);
uint8_t TH_isValid(void);

#endif /* SRC_AP_TEMPHUMID_TEMPHUMID_H_ */
