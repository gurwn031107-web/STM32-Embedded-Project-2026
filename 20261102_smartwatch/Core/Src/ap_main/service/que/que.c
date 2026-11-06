/*
 * que.c
 *
 *  Created on: 2026. 11. 2.
 *      Author: kccistc
 */


#include "que.h"


btn_event_t btn_event[10];
event_btn_act_t btn_mode[4];

que_buf_t que_mode;
que_buf_t que_funct_btn;

void que_init()
{
	que_buf_init(&que_mode, btn_mode, sizeof(event_btn_act_t), 4);
	que_buf_init(&que_funct_btn, btn_event, sizeof(btn_event_t), 10);
}

void que_putMode(event_btn_act_t act)
{
	que_put(&que_mode, &act);
}

void que_putFucntBtn(btn_event_t btn_evt)
{
	que_put(&que_funct_btn, &btn_evt);
}

bool que_isEmptyMode(void)
{
	return que_is_empty(&que_mode);
}

bool que_isEmptyFunctBtn(void)
{
	return que_is_empty(&que_funct_btn);
}

void que_getMode(event_btn_act_t *act)
{
	que_get(&que_mode, act);
}

void que_getFunctBtn(btn_event_t btn_evt)
{
	que_get(&que_funct_btn, &btn_evt);
}





