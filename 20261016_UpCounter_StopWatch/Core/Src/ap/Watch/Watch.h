/*
 * Watch.h
 *
 *  Created on: 2026. 10. 17.
 */

#ifndef SRC_AP_WATCH_WATCH_H_
#define SRC_AP_WATCH_WATCH_H_

void Watch_init(void);
void Watch_tick(void);	// TIM3 인터럽트(1ms)에서 호출 : 시간 증가
void Watch_task(void);	// main 루프에서 호출 : FND 표시

#endif /* SRC_AP_WATCH_WATCH_H_ */
