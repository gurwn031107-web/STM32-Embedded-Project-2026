/*
 * sensor_task.c
 *
 *  Created on: 2026. 11. 12.
 *
 *  초음파 센서 3개를 "한 번 호출에 3개 모두" 측정한다. (깃허브 방식)
 *    왼쪽 측정 → 15ms 쉬기 → 중앙 측정 → 15ms 쉬기 → 오른쪽 측정
 *    → 세 값을 필터 거친 뒤 한꺼번에 저장
 *  오른쪽 다음 쉬는 시간은 StartSensorTask의 osDelay(20)이 맡는다.
 *  센서끼리 15~20ms 간격이 생겨서 초음파가 서로 섞이는(간섭) 것을 줄일 수 있다.
 *  한 바퀴(3개 측정) 약 50ms + 측정 시간
 *
 *  [LCD 2번째 줄]  L / C / R 거리[cm]. 측정 실패 시 원인 코드 표시
 *    E1 : ECHO가 안 올라옴   → Trig/Echo/VCC/GND 배선, 센서 불량
 *    (E2 : 2m 안에 물체 없음 → 에러로 안 띄우고 최대값 DIST_MAX_CM(200)으로 표시)
 *    E3 : ECHO가 계속 High   → 센서 멈춤, 센서 전원 다시 연결
 *
 *  [배선]
 *    센서     TRIG (Output)    ECHO (Input)
 *    중앙     PB10 (D6)        PA8 (D7)
 *    왼쪽     PA4  (A2)        PB0 (A3)   ← A0/A1 핀 접촉 불량으로 옮김
 *    오른쪽   PA10 (D2)        PB3 (D3)
 */

#include "sensor_task.h"
#include "cmsis_os.h"
#include "../../../driver/ultrasonic/ultrasonic.h"
#include "../../../driver/lcd/lcd.h"
#include "../../auto_message.h"
#include "../../message.h"   // lcdMutexHandle 사용
#include <stdio.h>

// ultrasonic.h에 에러 코드가 없을 때를 대비 (있으면 ultrasonic.h 값을 그대로 사용)
#ifndef ULTRASONIC_ERR_NO_START
#define ULTRASONIC_ERR_NO_START  901  // E1
#endif
#ifndef ULTRASONIC_ERR_TOO_LONG
#define ULTRASONIC_ERR_TOO_LONG  902  // E2
#endif
#ifndef ULTRASONIC_ERR_BUSY
#define ULTRASONIC_ERR_BUSY      903  // E3
#endif

#define SENSOR_LCD_ENABLE   1     // 1: LCD 2번째 줄에 거리 표시, 0: 표시 안 함
#define LCD_UPDATE_MS       300   // LCD 표시 주기 [ms]
#define SENSOR_GAP_MS       15    // 센서와 센서 사이 쉬는 시간 [ms] (깃허브와 동일)
#define MISS_LIMIT          3     // 연속 측정 실패 몇 번까지 마지막 값을 유지할지 (넘으면 200 = 장애물 없음)
#define RETRY_MAX           3     // 측정 실패(E1/E2/E3) 시 바로 다시 재는 횟수 (최대 4번 측정)
#define RETRY_GAP_MS        10    // 다시 재기 전 쉬는 시간 [ms] (앞 초음파 소리가 사라지도록)

static ultrasonic_t us_center, us_left, us_right;
static distance_t dist = {DIST_MAX_CM, DIST_MAX_CM, DIST_MAX_CM};
static uint16_t raw_center = 0, raw_left = 0, raw_right = 0; // 필터 전 측정값 (에러 표시용)
static uint8_t miss_center = 0, miss_left = 0, miss_right = 0; // 연속 측정 실패 횟수

static uint16_t filter_distance(uint16_t prev, uint16_t now, uint8_t *miss)
{
	if (now == 0 || now > DIST_MAX_CM) {
		if (*miss < MISS_LIMIT) {
			(*miss)++;
			return prev;           // 잠깐 실패 → 마지막 정상값 유지
		}
		return DIST_MAX_CM;        // 계속 실패 → 장애물 없음
	}
	*miss = 0;

	if (now < prev) {
		return now;                // 가까워짐 → 바로 반영 (벽/코너에 늦게 반응하지 않도록)
	}

	if (prev == 0) {
		return now;
	}

	uint16_t diff = (now > prev) ? (now - prev) : (prev - now);
	if (diff > (prev * 3) / 10) {
		return now;
	}
	return (prev + now) / 2;
}

/*
 * LCD 표시용 3글자 만들기
 *  - 측정 성공 : 거리 숫자     예) " 32"
 *  - 측정 실패 : 에러 코드     예) " E1"  (원인은 ultrasonic.h 참고)
 */
static void make_lcd_text(char *out, uint16_t raw, uint16_t filtered)
{
	if (raw == ULTRASONIC_ERR_NO_START)      sprintf(out, " E1");
	else if (raw == ULTRASONIC_ERR_TOO_LONG) sprintf(out, " E2");
	else if (raw == ULTRASONIC_ERR_BUSY)     sprintf(out, " E3");
	else                                     sprintf(out, "%3d", filtered);
}

/*
 * 센서 1개 측정 (실패하면 다시 재기)
 *  - E2(2m 안에 물체 없음)나 DIST_MAX_CM보다 먼 값 → 에러가 아니라 "멀다" → DIST_MAX_CM(200)
 *  - 측정 실패(0, E1, E3)면 RETRY_GAP_MS 쉬고 다시 측정, 최대 RETRY_MAX번
 *  - 한 번이라도 정상값이 나오면 바로 그 값을 사용
 *  - 끝까지 실패하면 마지막 에러 코드 반환 (→ 필터가 마지막 정상값 유지 / LCD에 E 표시)
 */
static uint16_t measure_retry(ultrasonic_t *us)
{
	uint16_t d = ultrasonic_getDistance(us);

	for (int i = 0; i < RETRY_MAX; i++) {
		if (d == ULTRASONIC_ERR_TOO_LONG) {
			return DIST_MAX_CM;    // E2: 앞에 아무것도 없음 → 최대값 (다시 잴 필요 없음)
		}
		if (d != 0 && d < ULTRASONIC_ERR_NO_START) {
			break;                 // 정상값 → 끝
		}
		osDelay(RETRY_GAP_MS);     // 잠깐 쉬고
		d = ultrasonic_getDistance(us);   // 다시 측정
	}

	if (d == ULTRASONIC_ERR_TOO_LONG) {
		return DIST_MAX_CM;        // 마지막 측정이 E2여도 최대값
	}
	if (d != 0 && d < ULTRASONIC_ERR_NO_START && d > DIST_MAX_CM) {
		return DIST_MAX_CM;        // 최대값보다 먼 값은 최대값으로
	}
	return d;
}

void sensor_task_init(void)
{
	//                      TRIG                 ECHO
	ultrasonic_init(&us_center, GPIOB, GPIO_PIN_10, GPIOA, GPIO_PIN_8);
	ultrasonic_init(&us_left,   GPIOA, GPIO_PIN_4,  GPIOB, GPIO_PIN_0);
	ultrasonic_init(&us_right,  GPIOA, GPIO_PIN_10, GPIOB, GPIO_PIN_3);
}

void sensor_task(void)
{
	static uint32_t lcd_tick = 0;

	// 1) 왼쪽 → 중앙 → 오른쪽 순서로 3개 모두 측정 (사이사이 15ms 쉬기)
	//    실패하면 정상값이 나올 때까지 최대 3번 더 측정
	raw_left = measure_retry(&us_left);
	osDelay(SENSOR_GAP_MS);
	raw_center = measure_retry(&us_center);
	osDelay(SENSOR_GAP_MS);
	raw_right = measure_retry(&us_right);
	// 오른쪽 다음 쉬는 시간은 StartSensorTask의 osDelay(20)

	// 2) 필터 거친 뒤 3개를 한꺼번에 저장 (뮤텍스 보호는 distance_write 안에서)
	dist.left   = filter_distance(dist.left,   raw_left,   &miss_left);
	dist.center = filter_distance(dist.center, raw_center, &miss_center);
	dist.right  = filter_distance(dist.right,  raw_right,  &miss_right);
	distance_write(&dist);

	// 3) LCD 2번째 줄에 거리 표시   예) "L 32 C120 R 45"
#if SENSOR_LCD_ENABLE
	if (HAL_GetTick() - lcd_tick >= LCD_UPDATE_MS) {
		lcd_tick = HAL_GetTick();

		char l[8], c[8], r[8], buf[32];
		make_lcd_text(l, raw_left,   dist.left);
		make_lcd_text(c, raw_center, dist.center);
		make_lcd_text(r, raw_right,  dist.right);
		sprintf(buf, "L%s C%s R%s  ", l, c, r);   // 예) "L E1 C 25 R 30  "
		if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
			Lcd_writeStringXY(1, 0, buf);
			osMutexRelease(lcdMutexHandle);
		}
	}
#endif
}
