/*
 * auto_drive.c
 *
 *  Created on: 2026. 11. 12.
 *
 *  자동 주행 알고리즘 (참고: 3-Way-RC-Car freertos.c StartMotorTask 자율주행 부분)
 *
 *  [출발 대기]
 *   자동 모드가 시작되면 AUTO_START_DELAY_MS(2초) 동안 정지한 채 기다린다.
 *   → 버튼을 누르고 차를 바닥에 내려놓을 시간
 *
 *  [판단 우선순위]
 *   1) 전방 장애물 (중앙 < FRONT_STOP_CM)
 *        → 정지 (STOP_BEFORE_SPIN_MS 동안, 센서 3개 새로 측정)
 *        → 좌/우 중 더 넓은 쪽으로 제자리 회전
 *          (좌우 차이가 AMBIGUOUS_CM 보다 작아 넓은 쪽을 모르면 → 지난번에 돈 방향으로)
 *        → 중앙이 FRONT_CLEAR_CM 보다 멀어질 때까지 같은 방향으로 계속 회전
 *          (기준을 두 개로 나눠서 왼쪽/오른쪽을 왔다 갔다 하는 것을 막음 = 히스테리시스)
 *        → 정지 (STOP_AFTER_SPIN_MS) 후 다시 출발
 *   2) 측면이 너무 가까움 (왼쪽 or 오른쪽 < SIDE_EMERGENCY_CM)
 *        → 반대쪽으로 제자리 회전
 *   3) 안전
 *        → 좌우 거리 차이로 P제어 하면서 직진 (복도 가운데 유지)
 *
 *  [P제어]  양쪽 벽이 모두 SIDE_WALL_CM 안에 있을 때(복도)만 사용, 넓은 곳에서는 그냥 직진
 *          (넓은 곳에서는 옆 센서 값이 400과 엉뚱한 반사값 사이를 오가서 지그재그로 가기 때문)
 *   error  = 왼쪽거리 - 오른쪽거리     (+면 왼쪽이 넓음 = 오른쪽 벽에 가까움)
 *   offset = error x P_GAIN            (최대 ±MAX_OFFSET)
 *   왼쪽 바퀴 = BASE_SPEED - offset    → 왼쪽이 느려지고
 *   오른쪽 바퀴 = BASE_SPEED + offset  → 오른쪽이 빨라져서 왼쪽으로 휘어짐
 */

#include "auto_drive.h"
#include "cmsis_os.h"
#include "../auto_message.h"
#include "../message.h"                 // lcdMutexHandle 사용
#include "../../driver/lcd/lcd.h"

/* ================= 튜닝 값 (실제 주행하면서 조절) ================= */
#define FRONT_STOP_CM       20    // 전방 이 거리보다 가까우면 회피 시작 [cm] (좁은 트랙용, 원래 25)
#define FRONT_CLEAR_CM      30    // 전방 이 거리보다 멀어지면 회피 끝 [cm] (좁은 트랙용, 원래 35)
//#define SIDE_EMERGENCY_CM   10    // 측면 이 거리보다 가까우면 긴급 회피 [cm]
#define SIDE_LEFT_CM        27    // 좌회전 회피 기준: 오른쪽 벽이 이보다 가까우면 왼쪽으로 회피 [cm]
#define SIDE_RIGHT_CM       10    // 우회전 회피 기준: 왼쪽 벽이 이보다 가까우면 오른쪽으로 회피 [cm]
#define SIDE_MAX_CM         60    // P제어에 쓸 측면 거리 최대값 [cm]
                                  // (한쪽이 뻥 뚫려 있어도 너무 크게 꺾지 않도록)
#define BASE_SPEED          900   // 직진 기본 속도 (PWM 0~999)
#define SPIN_SPEED          700   // 제자리 회전 속도
#define P_GAIN              15    // 거리 오차 1cm당 속도 보정량
#define MAX_OFFSET          200   // 속도 보정 최대값
#define DEAD_BAND_CM        2     // 이 정도 오차는 무시 (좌우로 흔들리는 것 방지)
#define AUTO_START_DELAY_MS 0  // 자동 시작 후 출발까지 대기 시간 [ms]

#define SIDE_WALL_CM        40    // 양쪽이 모두 이보다 가까울 때만 "복도"로 보고 P제어 [cm]
                                  // (한쪽이 이보다 멀면 = 코너 출구가 열림 → 그쪽으로 끌려가지 않고 직진)
#define OPEN_CM             40    // 직진 중 옆 거리가 이보다 멀어지면 "그쪽이 열렸다(코너 출구)"로 기억 [cm]
#define OPEN_MEMORY_MS      2000  // 열린 쪽을 기억하는 시간 [ms] (이 안에 정면이 막히면 기억한 쪽으로 회전)
#define AMBIGUOUS_CM        10    // 좌우 차이가 이보다 작으면 넓은 쪽을 모름 → 지난번 방향 [cm]
#define STOP_BEFORE_SPIN_MS 200   // 회전 전 정지 시간 [ms] (센서당 60ms → 3바퀴 새로 측정), 0이면 바로 회전
#define STOP_AFTER_SPIN_MS  100   // 회전 후 정지 시간 [ms], 0이면 바로 출발

/* 바퀴별 속도 보정 (모터마다 힘이 달라서 같은 PWM이어도 한쪽으로 휠 때 사용)
 *  - 오른쪽으로 휘면 → RIGHT_TRIM을 + 로 (오른쪽 바퀴를 더 세게)
 *  - 왼쪽으로 휘면   → LEFT_TRIM을 + 로
 *  P_GAIN을 0으로 두고 똑바로 갈 때까지 조절한 뒤, P_GAIN을 다시 15로 되돌린다.
 */
#define LEFT_TRIM           0     // 왼쪽 바퀴 보정값 (PWM, + 면 빨라짐)
#define RIGHT_TRIM          50     // 오른쪽 바퀴 보정값 (PWM, + 면 빨라짐)
/* ================================================================ */

static auto_action_t last_action = AUTO_NONE;   // LCD는 동작이 바뀔 때만 갱신
/* 전방 회피 단계 */
typedef enum {
	AV_NONE,      // 회피 안 함 (직진 / 측면 판단)
	AV_STOP1,     // 회전 전 정지 (거리 새로 측정)
	AV_SPIN,      // 넓은 쪽으로 회전
	AV_STOP2,     // 회전 후 정지
} avoid_phase_t;
static avoid_phase_t phase = AV_NONE;
static uint32_t phase_tick = 0;                  // 현재 단계 시작 시각
static auto_action_t last_turn = AUTO_SPIN_LEFT; // 지난번 회전 방향 (넓은 쪽을 모를 때 사용)
static auto_action_t avoid_dir = AUTO_SPIN_LEFT; // 회피 중 회전 방향
static auto_action_t open_dir = AUTO_NONE;      // 직진 중 본 열린 쪽 (코너 출구), 없으면 AUTO_NONE
static uint32_t open_tick = 0;                  // 열린 쪽을 마지막으로 본 시각
static uint8_t  waiting = 0;                    // 1: 출발 대기 시작함
static uint8_t  driving = 0;                    // 1: 대기 끝, 주행 중
static uint32_t wait_tick = 0;                  // 대기 시작 시각

/* ---------------- 바퀴 하나씩 제어 ----------------
 * dc_motor.c의 방향 규칙을 그대로 따름
 *   왼쪽 모터  전진: GPIO1=SET,   GPIO2=RESET / 후진: 반대
 *   오른쪽 모터 전진: GPIO1=RESET, GPIO2=SET   / 후진: 반대
 * (dc_motor.c는 두 바퀴에 같은 속도만 줄 수 있어서, 바퀴마다 속도를 다르게 주는 함수를 여기 만듦)
 */
static uint16_t clamp_speed(int speed)
{
	if (speed < 0) return 0;
	if (speed > 999) return 999;
	return (uint16_t)speed;
}

static void wheel(motor_t *motor, GPIO_PinState pin1, GPIO_PinState pin2, int speed)
{
	__HAL_TIM_SET_COMPARE(motor->htim, motor->Ch, clamp_speed(speed));
	HAL_GPIO_WritePin(motor->GPIO1, motor->GPIO1_Pin, pin1);
	HAL_GPIO_WritePin(motor->GPIO2, motor->GPIO2_Pin, pin2);
	HAL_TIM_PWM_Start(motor->htim, motor->Ch);
}

// 바퀴별 보정값(LEFT_TRIM / RIGHT_TRIM)을 더해서 출력
static void left_forward(motor_t *m, int speed)   { wheel(m, GPIO_PIN_SET,   GPIO_PIN_RESET, speed + LEFT_TRIM); }
static void left_backward(motor_t *m, int speed)  { wheel(m, GPIO_PIN_RESET, GPIO_PIN_SET,   speed + LEFT_TRIM); }
static void right_forward(motor_t *m, int speed)  { wheel(m, GPIO_PIN_RESET, GPIO_PIN_SET,   speed + RIGHT_TRIM); }
static void right_backward(motor_t *m, int speed) { wheel(m, GPIO_PIN_SET,   GPIO_PIN_RESET, speed + RIGHT_TRIM); }

/* ---------------- LCD 표시 (1번째 줄) ---------------- */
static void lcd_show_action(auto_action_t action)
{
	char *str;

	switch (action) {
	case AUTO_FORWARD:    str = "AUTO FORWARD    "; break;
	case AUTO_SPIN_LEFT:  str = "AUTO SPIN LEFT  "; break;
	case AUTO_SPIN_RIGHT: str = "AUTO SPIN RIGHT "; break;
	case AUTO_PAUSE:      str = "AUTO PAUSE      "; break;
	default:              str = "AUTO STOP       "; break;
	}

	if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
		Lcd_writeStringXY(0, 0, str);
		osMutexRelease(lcdMutexHandle);
	}
}

/* ---------------- 자동 주행 1회 ---------------- */
auto_action_t auto_drive_update(motor_t *motor_l, motor_t *motor_r)
{
	distance_t d;
	auto_action_t action;

	// 0) 출발 대기 (자동 모드 시작 후 AUTO_START_DELAY_MS 동안 정지)
	if (!driving) {
		if (!waiting) {                       // 대기 처음 시작
			waiting = 1;
			wait_tick = HAL_GetTick();
			motor_stop(motor_l, motor_r);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(0, 0, "AUTO READY...   ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		if (HAL_GetTick() - wait_tick < AUTO_START_DELAY_MS) {
			return AUTO_NONE;                 // 아직 대기 중
		}
		driving = 1;                          // 대기 끝 → 주행 시작
	}

	// 1) 최신 거리값 가져오기 (sensorMutex 보호)
	distance_read(&d);

	// 2) 전방 장애물 회피 단계 진행
	uint32_t now = HAL_GetTick();

	// 2-0) 코너에 다가가는 동안 "한쪽만 열린 것"을 기억 (직각 코너 출구 찾기)
	//      양쪽 다 열리거나 다 막혀 있으면 기억하지 않음
	if (phase == AV_NONE || phase == AV_STOP1) {
		uint8_t l_open = (d.left  >= OPEN_CM);
		uint8_t r_open = (d.right >= OPEN_CM);
		if (l_open && !r_open) {
			open_dir = AUTO_SPIN_LEFT;
			open_tick = now;
		}
		else if (r_open && !l_open) {
			open_dir = AUTO_SPIN_RIGHT;
			open_tick = now;
		}
	}

	switch (phase)
	{
	case AV_NONE:                         // 앞이 막혔는지 확인
		if (d.center < FRONT_STOP_CM) {
			phase = AV_STOP1;
			phase_tick = now;
		}
		break;

	case AV_STOP1:                        // 멈춘 채로 센서가 새로 잴 때까지 기다린 뒤 방향 결정
		if (now - phase_tick >= STOP_BEFORE_SPIN_MS) {
			int diff = d.left - d.right;
			if (open_dir != AUTO_NONE && (now - open_tick) <= OPEN_MEMORY_MS) {
				avoid_dir = open_dir;     // 1순위: 다가오면서 본 열린 쪽 (코너 출구)
			}
			else if (diff > -AMBIGUOUS_CM && diff < AMBIGUOUS_CM) {
				avoid_dir = last_turn;    // 3순위: 넓은 쪽을 모름 → 지난번과 같은 방향
			}
			else {
				avoid_dir = (diff > 0) ? AUTO_SPIN_LEFT : AUTO_SPIN_RIGHT; // 2순위: 지금 더 넓은 쪽
			}
			open_dir = AUTO_NONE;         // 한 번 썼으면 지움 (다음 코너는 새로 찾기)
			last_turn = avoid_dir;
			phase = AV_SPIN;
		}
		break;

	case AV_SPIN:                         // 앞이 충분히 열릴 때까지 같은 방향으로 회전
		if (d.center > FRONT_CLEAR_CM) {
			phase = AV_STOP2;
			phase_tick = now;
		}
		break;

	case AV_STOP2:                        // 잠깐 멈춘 뒤 다시 출발
		if (now - phase_tick >= STOP_AFTER_SPIN_MS) {
			phase = AV_NONE;
		}
		break;
	}

	// 3) 동작 결정 (우선순위 순서)
	if (phase == AV_STOP1 || phase == AV_STOP2) {
		action = AUTO_PAUSE;              // 1순위: 전방 회피 (정지)
	}
	else if (phase == AV_SPIN) {
		action = avoid_dir;               // 1순위: 전방 회피 (회전)
	}
	else if (d.left < SIDE_RIGHT_CM) {
		action = AUTO_SPIN_RIGHT;         // 2순위: 왼쪽 벽에 너무 가까움
	}
	else if (d.right < SIDE_LEFT_CM) {
		action = AUTO_SPIN_LEFT;          // 2순위: 오른쪽 벽에 너무 가까움
	}
	else {
		action = AUTO_FORWARD;            // 3순위: P제어 직진
	}

	// 4) 모터 출력
	switch (action)
	{
	case AUTO_SPIN_LEFT:       // 왼쪽 바퀴 후진 + 오른쪽 바퀴 전진
		left_backward(motor_l, SPIN_SPEED);
		right_forward(motor_r, SPIN_SPEED);
		break;
	case AUTO_SPIN_RIGHT:      // 왼쪽 바퀴 전진 + 오른쪽 바퀴 후진
		left_forward(motor_l, SPIN_SPEED);
		right_backward(motor_r, SPIN_SPEED);
		break;
	case AUTO_PAUSE:           // 회전 전후 잠깐 정지
		motor_stop(motor_l, motor_r);
		break;
	case AUTO_FORWARD:
	{
		int l = (d.left  > SIDE_MAX_CM) ? SIDE_MAX_CM : d.left;
		int r = (d.right > SIDE_MAX_CM) ? SIDE_MAX_CM : d.right;
		int error = 0;

		// 양쪽 벽이 다 있을 때(복도)만 가운데 맞추기, 넓은 곳에서는 그냥 직진
		if (d.left < SIDE_WALL_CM && d.right < SIDE_WALL_CM) {
			error = l - r;
		}

		if (error >= -DEAD_BAND_CM && error <= DEAD_BAND_CM) {
			error = 0;
		}

		int offset = error * P_GAIN;
		if (offset >  MAX_OFFSET) offset =  MAX_OFFSET;
		if (offset < -MAX_OFFSET) offset = -MAX_OFFSET;

		left_forward(motor_l,  BASE_SPEED - offset);
		right_forward(motor_r, BASE_SPEED + offset);
		break;
	}
	default:
		motor_stop(motor_l, motor_r);
		break;
	}

	// 5) 동작이 바뀌었을 때만 LCD 갱신 (매번 쓰면 I2C가 느려서 낭비)
	if (action != last_action) {
		lcd_show_action(action);
		last_action = action;
	}

	return action;
}

/* ---------------- 정지 + 상태 초기화 ----------------
 * 모드가 바뀔 때 호출 → 다음에 자동 모드로 들어오면 처음 상태부터 시작
 */
void auto_drive_stop(motor_t *motor_l, motor_t *motor_r)
{
	motor_stop(motor_l, motor_r);
	phase = AV_NONE;
	open_dir = AUTO_NONE;
	last_action = AUTO_NONE;
	waiting = 0;     // 다음에 자동 모드 들어오면 다시 2초 대기부터
	driving = 0;
}
