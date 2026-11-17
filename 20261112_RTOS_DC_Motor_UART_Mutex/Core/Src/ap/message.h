/*
 * message.h
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */

#ifndef SRC_AP_MESSAGE_H_
#define SRC_AP_MESSAGE_H_

#include "cmsis_os.h"

typedef enum {BTN_NO, BTN_RUN_STOP, BTN_LEFT, BTN_RIGHT} btn_status_t;
extern osMessageQId btnQueHandle;

typedef enum {COMM_NO, COMM_RUN, COMM_STOP, COMM_SPEED} comm_status_t;

typedef struct {
	comm_status_t status;
	int speed;
} comm_data_t;

extern osMessageQId commQueueHandle;

extern osMutexId lcdMutexHandle;

#endif /* SRC_AP_MESSAGE_H_ */
