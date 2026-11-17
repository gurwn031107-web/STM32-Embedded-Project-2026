/*
 * auto_message.h
 *
 *  Created on: 2026. 11. 12.
 *
 *  자동 주행 구조에서 태스크끼리 같이 쓰는 데이터 정의
 *  (기존 message.h는 수정하지 않고, 자동 구조용으로 따로 만든 파일)
 *
 *  [데이터 흐름]
 *    SensorTask ──(distance_t, sensorMutex로 보호)──> MotorTask (auto_drive)
 *    CommTask   ──(drive_mode_set)──────────────────> MotorTask (모드 확인)
 */

#ifndef SRC_AP_AUTO_MESSAGE_H_
#define SRC_AP_AUTO_MESSAGE_H_

#include "cmsis_os.h"
#include <stdint.h>

/* ---------------- 주행 모드 ---------------- */
typedef enum {MODE_MANUAL, MODE_AUTO} drive_mode_t;

void drive_mode_set(drive_mode_t mode);
drive_mode_t drive_mode_get(void);

/* ---------------- 초음파 거리 ---------------- */
#define DIST_MAX_CM   200   // 센서 최대값 [cm]. 이보다 멀거나 반사파가 없으면(E2) 이 값으로 처리

typedef struct {
	uint16_t left;    // 왼쪽 센서 [cm]
	uint16_t center;  // 중앙 센서 [cm]
	uint16_t right;   // 오른쪽 센서 [cm]
} distance_t;

void distance_write(distance_t *src); // SensorTask가 사용
void distance_read(distance_t *dst);  // MotorTask(auto_drive)가 사용

extern osMutexId sensorMutexHandle;   // CubeMX에서 만든 sensorMutex

#endif /* SRC_AP_AUTO_MESSAGE_H_ */
