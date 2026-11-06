/*
 * que.h
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */

#ifndef SRC_COMMON_QUE_QUE_H_
#define SRC_COMMON_QUE_QUE_H_

#include "../../../common/queue/que_buffer.h"
#include <stdbool.h>

typedef enum {BTN_FUCNT, BTN_CLEAR} event_btn_t;
typedef enum {ACT_PUSHED, ACT_RELEASED} event_btn_act_t;
typedef struct {
	event_btn_t evt_btn;
	event_btn_act_t event_btn_act;
} btn_event_t;

void que_init();
void que_putMode(event_btn_act_t act);
void que_putFucntBtn(btn_event_t btn_evt);
bool que_isEmptyMode(void);
bool que_isEmptyFunctBtn(void);
void que_getMode(event_btn_act_t *act);
void que_getFunctBtn(btn_event_t btn_evt);

#endif /* SRC_COMMON_QUE_QUE_H_ */
