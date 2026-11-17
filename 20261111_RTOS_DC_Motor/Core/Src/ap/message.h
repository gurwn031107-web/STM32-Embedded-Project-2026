/*
 * message.h
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */

#ifndef SRC_AP_MESSAGE_H_
#define SRC_AP_MESSAGE_H_

#include "cmsis_os.h"

typedef enum {
	BTN_NO,
	BTN_RUN_STOP,
	BTN_LEFT,
	BTN_RIGHT
} btn_status_t;
extern osMessageQId btnQueHandle;

#endif /* SRC_AP_MESSAGE_H_ */
