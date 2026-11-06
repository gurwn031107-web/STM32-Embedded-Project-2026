/*
 * stopWatch.h
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#ifndef SRC_AP_MAIN_SERVICE_STOPWATCH_STOPWATCH_H_
#define SRC_AP_MAIN_SERVICE_STOPWATCH_STOPWATCH_H_

void stopWatch_init(void);
void stopWatch_task(void);
void stopWatch_ISR(void);
void stopWatch_disp(void);

#endif /* SRC_AP_MAIN_SERVICE_STOPWATCH_STOPWATCH_H_ */
