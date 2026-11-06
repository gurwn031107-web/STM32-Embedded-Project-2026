/*
 * Distance.h
 *
 *  초음파 거리 서비스 task
 *  - 100ms마다 HC-SR04로 거리를 측정해서 최신 값을 보관
 *  - 응용(display 등)은 센서를 직접 읽지 않고 Distance_get()으로 값만 가져감
 */

#ifndef SRC_AP_SERVICE_DISTANCE_DISTANCE_H_
#define SRC_AP_SERVICE_DISTANCE_DISTANCE_H_

#include "stm32f4xx_hal.h"
#include "../../../driver/HCSR04/HCSR04.h"

void Distance_init(void);
void Distance_task(void);
void Distance_echoISR(void);
hcsr04_status_t Distance_get(uint32_t *distance_mm);

#endif /* SRC_AP_SERVICE_DISTANCE_DISTANCE_H_ */
