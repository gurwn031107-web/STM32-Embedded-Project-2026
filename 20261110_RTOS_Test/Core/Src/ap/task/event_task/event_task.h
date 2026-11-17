/*
 * event_task.h
 *
 *  Created on: 2026. 11. 10.
 *      Author: kccistc
 */

#ifndef SRC_AP_TASK_EVENT_TASK_EVENT_TASK_H_
#define SRC_AP_TASK_EVENT_TASK_EVENT_TASK_H_


typedef enum {
	BTN_NO,
	BTN_RUN_STOP,
	BTN_CLEAR
} btn_status_t;

extern btn_status_t btn_status; // btn_status = 공유 메모리

void event_task_init(void);
void event_task(void);

#endif /* SRC_AP_TASK_EVENT_TASK_EVENT_TASK_H_ */
