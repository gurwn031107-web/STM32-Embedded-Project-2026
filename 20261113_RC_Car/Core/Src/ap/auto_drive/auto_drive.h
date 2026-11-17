/*
 * auto_drive.h
 *
 *  Created on: 2026. 11. 12.
 *
 *  자동(자율) 주행 판단 + 모터 출력
 *  MotorTask(motor_control_task)가 자동 모드일 때 주기적으로 auto_drive_update()를 호출한다.
 *  (motor_l, motor_r 은 motor_control_task.c 안에 있으므로 포인터로 넘겨받는다)
 */

#ifndef SRC_AP_AUTO_DRIVE_AUTO_DRIVE_H_
#define SRC_AP_AUTO_DRIVE_AUTO_DRIVE_H_

#include "../../driver/dc_motor/dc_motor.h"

typedef enum {
	AUTO_NONE,         // 아직 동작 안 함 (처음 상태)
	AUTO_FORWARD,      // 직진 (좌우 거리 P제어)
	AUTO_SPIN_LEFT,    // 제자리 좌회전
	AUTO_SPIN_RIGHT,   // 제자리 우회전
	AUTO_PAUSE,        // 회전 전후 잠깐 정지
} auto_action_t;

auto_action_t auto_drive_update(motor_t *motor_l, motor_t *motor_r); // 자동 주행 1회 판단 + 출력
void auto_drive_stop(motor_t *motor_l, motor_t *motor_r);            // 정지 + 자동주행 상태 초기화

#endif /* SRC_AP_AUTO_DRIVE_AUTO_DRIVE_H_ */
