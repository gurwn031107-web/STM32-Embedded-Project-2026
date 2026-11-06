/*
 * distance.h
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#ifndef SRC_AP_MAIN_SERVICE_DISTANCE_DISTANCE_H_
#define SRC_AP_MAIN_SERVICE_DISTANCE_DISTANCE_H_

#include <stdint.h>

void distance_init(void);
void distance_disp(uint16_t distance);
void distance_task(void);
void distance_ISR(void);

#endif /* SRC_AP_MAIN_SERVICE_DISTANCE_DISTANCE_H_ */
