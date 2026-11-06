/*
 * fnd.h
 *
 * 4자리 FND(공통 캐소드) 드라이버
 *   세그먼트 : A=PA6  B=PA9  C=PB5  D=PB10  E=PA8  F=PA7  G=PB3  DP=PB4  (SET = 켜짐)
 *   자리     : D1=PA5  D2=PB6  D3=PC7  D4=PA10                         (RESET = 켜짐)
 */

#ifndef DRIVER_FND_FND_H_
#define DRIVER_FND_FND_H_

#include "stm32f4xx_hal.h"

void fnd_show_number(int num);

#endif /* DRIVER_FND_FND_H_ */
