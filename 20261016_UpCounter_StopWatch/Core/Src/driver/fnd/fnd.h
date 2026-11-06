/*
 * fnd.h
 *
 *  Created on: 2026. 10. 14.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_FND_FND_H_
#define SRC_DRIVER_FND_FND_H_

#include "stm32f4xx_hal.h"


#define FND_DP_NONE	0x00
#define FND_DP_1	0x01	// 1번째 자리
#define FND_DP_2	0x02	// 2번째 자리
#define FND_DP_3	0x04	// 3번째 자리
#define FND_DP_4	0x08	// 4번째 자리

void fnd_set_num(uint16_t n);
uint16_t fnd_get_num(void);
void fnd_set_dp(uint8_t dp);
uint8_t fnd_get_dp(void);
void fnd_show_number();


#endif /* SRC_DRIVER_FND_FND_H_ */
