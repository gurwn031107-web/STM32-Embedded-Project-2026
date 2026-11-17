/*
 * auto_message.c
 *
 *  Created on: 2026. 11. 12.
 */

#include "auto_message.h"

/* ---------------- 주행 모드 ----------------
 * CommTask가 쓰고 MotorTask가 읽는다.
 * 32비트 변수 1개를 읽고 쓰는 것은 CPU가 한 번에 처리하므로 뮤텍스 없이 안전하다.
 * volatile : 다른 태스크가 바꾼 값을 항상 메모리에서 새로 읽도록 함
 */
static volatile drive_mode_t drive_mode = MODE_MANUAL;

void drive_mode_set(drive_mode_t mode)
{
	drive_mode = mode;
}

drive_mode_t drive_mode_get(void)
{
	return drive_mode;
}

/* ---------------- 초음파 거리 ----------------
 * left / center / right 3개 값을 한 묶음으로 주고받는다.
 * SensorTask가 쓰는 도중에 MotorTask가 읽으면 값이 섞일 수 있으므로
 * sensorMutex로 보호한다. (LCD를 lcdMutex로 보호하는 것과 같은 원리)
 */
static distance_t distance = {DIST_MAX_CM, DIST_MAX_CM, DIST_MAX_CM};

void distance_write(distance_t *src)
{
	if (osMutexWait(sensorMutexHandle, osWaitForever) == osOK) {
		distance = *src;
		osMutexRelease(sensorMutexHandle);
	}
}

void distance_read(distance_t *dst)
{
	if (osMutexWait(sensorMutexHandle, osWaitForever) == osOK) {
		*dst = distance;
		osMutexRelease(sensorMutexHandle);
	}
}
