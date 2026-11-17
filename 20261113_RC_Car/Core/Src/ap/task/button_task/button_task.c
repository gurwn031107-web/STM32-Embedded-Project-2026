/*
 * button_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */

#include "button_task.h"
#include "cmsis_os.h"
#include "../../../driver/button/button.h"
#include "../../../driver/lcd/lcd.h"
#include "../../message.h" // enum 자료형을 가져다가 사용.
#include "../../auto_message.h" // [자동 구조 추가] drive_mode_get/set

static button_t btn_run_stop, btn_left, btn_right;

void button_task_init(void)
{
	button_init(&btn_run_stop, GPIOC, GPIO_PIN_0);
	button_init(&btn_left, GPIOC, GPIO_PIN_1);
	button_init(&btn_right, GPIOC, GPIO_PIN_2);
}

void button_task(void)
{
	if (button_getState(&btn_run_stop) == ACT_RELEASE) {
		/* ===== [자동 구조 변경] 1번 버튼 = 자동 주행 시작 / 정지 =====
		 * 원래 코드 (수동 RUN/STOP):
		 *   osMessagePut(btnQueHandle, BTN_RUN_STOP, 10);
		 *   Lcd_writeStringXY(1, 0, "Button Task RUN_STOP");
		 */
		if (drive_mode_get() == MODE_MANUAL) {
			drive_mode_set(MODE_AUTO);     // 수동 → 자동 시작 (2초 대기 후 출발)
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Button AUTO START");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else {
			drive_mode_set(MODE_MANUAL);   // 자동 → 정지 (수동 STOP 상태로)
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Button AUTO STOP ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		/* ===== [자동 구조 변경] 끝 ===== */
	}
	else if (button_getState(&btn_left) == ACT_RELEASE) {
		osMessagePut(btnQueHandle, BTN_LEFT, 10);
		if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
			Lcd_writeStringXY(1, 0, "Button Task LEFT");
			osMutexRelease(lcdMutexHandle);
		}
	}
	else if (button_getState(&btn_right) == ACT_RELEASE) {
		osMessagePut(btnQueHandle, BTN_RIGHT, 10);
		if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
			Lcd_writeStringXY(1, 0, "Button Task RIGHT");
			osMutexRelease(lcdMutexHandle);
		}
	}
}
