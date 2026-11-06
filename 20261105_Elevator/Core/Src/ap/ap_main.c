/*
 * ap_main.c  —  3층 엘리베이터 (쉬운 버전)
 *
 * [이 파일 읽는 순서]
 *   1. 설정값      : 숫자만 바꾸면 되는 값
 *   2. 변수        : 엘리베이터의 "지금 상태"
 *   3. 작은 함수   : 버튼 읽기 · 센서 읽기 · 부저 · 문 LED
 *   4. 동작 함수   : 출발 · 원점 잡기 · 도착(문 열기) · 에러 정지 · 층 호출
 *   5. ap_init     : 전원 켜면 한 번
 *   6. ap_main     : while(1)에서 계속 → 할 일 ①~⑦을 차례로 부름
 *   7. 인터럽트    : 타이머(모터 · 층 판정) · 센서 EXTI · UART 수신
 *
 * [동작 요약]
 *   버튼(또는 PC "CALL n") → 그 층으로 이동 → 센서가 60스텝 연속 가려지면 정지
 *   → 딩동 → 도착 2초 뒤 문 LED 3초 → 닫힘
 *   전원 ON → 1층까지 내려가 원점(위치 0) 잡기
 *   이동 중 버튼 → 비상 정지 / 너무 오래 걸리면 타임아웃 정지 → 버튼 누르면 원점부터 다시
 *
 * [역할 나누기]
 *   타이머 인터럽트(2ms) : 모터 1스텝 + 센서 확인 + 목표 층이면 정지   ← 시간이 정확해야 하는 일
 *   ap_main(계속 반복)   : 버튼 · PC 명령 · 문 · LCD · 온습도           ← 판단하고 보여 주는 일
 */

#include "ap_main.h"
#include <stdio.h>
#include <string.h>
#include "../driver/StepMotor/StepMotor.h"
#include "../driver/lcd1602/lcd1602.h"
#include "service/TempHumid/TempHumid.h"   // DHT11 온습도

extern TIM_HandleTypeDef  htim3;   // 1ms 인터럽트 → 모터
extern TIM_HandleTypeDef  htim1;   // 부저 PWM (CH4 = PA11), 카운터 1MHz
extern UART_HandleTypeDef huart2;  // PC (printf · GUI 명령)
extern I2C_HandleTypeDef  hi2c1;   // LCD


/* ============================================================
 * 1. 설정값
 * ============================================================ */
#define STEP_MS             2       // 2ms마다 모터 1스텝
#define STOP_CONFIRM_STEPS  60      // 센서가 60스텝 연속 가려지면 "그 층에 왔다"
#define TIMEOUT_STEPS       15000   // 이만큼 가도 목표 층이 안 나오면 타임아웃 (약 30초)
#define TOO_FAST_STEPS      100     // 출발하고 100스텝도 안 돼서 도착하면 센서 이상

#define DING_HZ             1319    // 딩 (미)
#define DONG_HZ             1047    // 동 (도)
#define DING_MS             300     // 딩 0.3초
#define DONG_MS             600     // 동 0.6초
#define DOOR_OPEN_AT_MS     2000    // 도착 2초 뒤 문 열림
#define DOOR_CLOSE_AT_MS    5000    // 도착 5초 뒤 문 닫힘 (= 3초 열림)

#define DIR_UP              STEP_MOTOR_DIR_CW    // 위아래가 반대로 움직이면 이 두 줄을 바꾸기
#define DIR_DOWN            STEP_MOTOR_DIR_CCW
#define SENSOR_BLOCKED      GPIO_PIN_SET         // 센서를 가리면 HIGH


/* ============================================================
 * 2. 변수
 * ============================================================ */

/* 버튼 · 센서 핀 — 배열 번호 [0] = 1층, [1] = 2층, [2] = 3층 */
static GPIO_TypeDef *btnPort[3] = { BTN_1F_GPIO_Port, BTN_2F_GPIO_Port, BTN_3F_GPIO_Port };
static uint16_t      btnPin[3]  = { BTN_1F_Pin,       BTN_2F_Pin,       BTN_3F_Pin       };
static GPIO_TypeDef *senPort[3] = { SENSOR_1F_GPIO_Port, SENSOR_2F_GPIO_Port, SENSOR_3F_GPIO_Port };
static uint16_t      senPin[3]  = { SENSOR_1F_Pin,       SENSOR_2F_Pin,       SENSOR_3F_Pin       };

/* 스텝모터 */
static stepMotor_pin_t motorPin = {
	.port = { MOTOR_IN1_GPIO_Port, MOTOR_IN2_GPIO_Port, MOTOR_IN3_GPIO_Port, MOTOR_IN4_GPIO_Port },
	.pin  = { MOTOR_IN1_Pin,       MOTOR_IN2_Pin,       MOTOR_IN3_Pin,       MOTOR_IN4_Pin       }
};
static stepMotor_t motor;

/* 엘리베이터 상태 (상태 머신) */
typedef enum {
	ST_HOMING,   // 1층 찾으러 내려가는 중 (원점 잡기)
	ST_IDLE,     // 층에 서서 호출 기다림
	ST_MOVING,   // 목표 층으로 가는 중
	ST_DOOR,     // 도착: 딩동 → 문 열림 → 닫힘
	ST_ERROR     // 멈춤 (비상 정지 · 타임아웃 · 센서 이상)
} state_t;

typedef enum { MOVE_STOP, MOVE_UP, MOVE_DOWN } move_t;

static state_t     state    = ST_HOMING;
static const char *errText  = "NONE";   // 에러 이름: "NONE" · "STOP" · "TIMEOUT" · "SENSOR"
static int8_t      errFloor = 0;        // 에러가 난 층 (LCD 표시용)
static uint8_t     lcdOk    = 0;        // LCD 연결됐으면 1

/* 문 */
static uint32_t doorTime  = 0;   // 도착한 시각 (ms)
static uint8_t  doorOpen  = 0;   // 문 LED 켜져 있으면 1
static int8_t   nextFloor = 0;   // 문 열려 있는 동안 눌린 층 (0 = 없음)

/* 타이머 인터럽트와 같이 쓰는 변수 → volatile (언제든 인터럽트가 바꿀 수 있음) */
static volatile move_t   moveCmd     = MOVE_STOP;  // 모터 방향 (인터럽트가 이걸 보고 돌림)
static volatile int32_t  position    = 0;          // 1층 = 0, 올라가면 +1
static volatile uint32_t moveSteps   = 0;          // 이번 이동에서 간 스텝 수
static volatile int8_t   curFloor    = 0;          // 지금 층 (0 = 모름)
static volatile int8_t   targetFloor = 0;          // 가야 할 층
static volatile uint8_t  arrived     = 0;          // 인터럽트 → main: "목표 층에 멈췄음"
static volatile uint16_t blkSteps[3];              // 센서가 연속으로 가려진 스텝 수
static volatile uint32_t sensorHits[3];            // 센서가 가려진 횟수 (EXTI, 확인용)

/* PC 명령 받기 (UART 인터럽트가 한 글자씩 받아 한 줄을 만듦) */
static uint8_t          rxByte;          // 방금 받은 한 글자
static char             rxBuf[20];       // 만드는 중인 줄
static uint8_t          rxLen = 0;
static char             cmdLine[20];     // 다 만든 한 줄 → main이 읽음
static volatile uint8_t cmdReady = 0;    // 1이면 cmdLine에 새 명령 있음


/* ============================================================
 * 3. 작은 함수
 * ============================================================ */

/* printf → UART2 (PC) */
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
	return ch;
}

/* 버튼 i가 눌렸나 (풀업이라 누르면 LOW) */
static int isButtonPressed(int i)
{
	return HAL_GPIO_ReadPin(btnPort[i], btnPin[i]) == GPIO_PIN_RESET;
}

/* 센서 i가 가려졌나 */
static int isSensorBlocked(int i)
{
	return HAL_GPIO_ReadPin(senPort[i], senPin[i]) == SENSOR_BLOCKED;
}

/* 가려진 센서 중 가장 낮은 층 (없으면 0) */
static int8_t findBlockedFloor(void)
{
	for (int i = 0; i < 3; i++) {
		if (isSensorBlocked(i)) return i + 1;
	}
	return 0;
}

/* 가려진 센서 개수 */
static int countBlocked(void)
{
	return isSensorBlocked(0) + isSensorBlocked(1) + isSensorBlocked(2);
}

/* 부저: hz = 0이면 끔. 같은 소리면 다시 설정 안 함
 *  주파수 = 1MHz ÷ (ARR + 1)  →  ARR = 1,000,000 ÷ hz − 1,  소리 크기(듀티 50%) = ARR의 절반 */
static void buzzer(uint32_t hz)
{
	static uint32_t lastHz = 0xFFFFFFFF;   // 처음 한 번은 무조건 설정
	if (hz == lastHz) return;
	lastHz = hz;

	if (hz == 0) {
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
		return;
	}
	uint32_t arr = 1000000 / hz - 1;
	__HAL_TIM_SET_AUTORELOAD(&htim1, arr);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (arr + 1) / 2);
}

/* 문 LED 켜기(1) / 끄기(0) */
static void doorLed(int on)
{
	HAL_GPIO_WritePin(LED_DOOR_GPIO_Port, LED_DOOR_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/* ============================================================
 * 4. 동작 함수
 * ============================================================ */

/* 모터 출발 — moveCmd를 맨 마지막에 바꿔야 인터럽트가 준비된 값으로 움직임 */
static void startMotor(int8_t target, move_t dir)
{
	targetFloor = target;
	moveSteps   = 0;
	blkSteps[0] = blkSteps[1] = blkSteps[2] = 0;
	arrived     = 0;
	moveCmd     = dir;
}

/* 다른 층으로 출발 */
static void goToFloor(int8_t floor)
{
	printf("MOVE %dF -> %dF\r\n", curFloor, floor);
	state = ST_MOVING;
	startMotor(floor, (floor > curFloor) ? MOVE_UP : MOVE_DOWN);
}

/* 원점 잡기: 1층이 맨 아래라서 항상 아래로만 내려가면 1층 센서를 만남 */
static void goHome(void)
{
	printf("HOMING: go down to 1F\r\n");
	errText = "NONE";
	state   = ST_HOMING;
	startMotor(1, MOVE_DOWN);
}

/* 도착 → 딩동부터 시작 (실제 소리 · 문은 runDoor()가 시간에 맞춰 처리) */
static void openDoor(void)
{
	printf("DING-DONG at %dF\r\n", curFloor);
	state     = ST_DOOR;
	doorTime  = HAL_GetTick();
	doorOpen  = 0;
	nextFloor = 0;
}

/* 에러 정지: 모터 즉시 멈춤 (코일은 켜 둬서 카가 안 미끄러짐) */
static void stopWithError(const char *name, int8_t floor)
{
	moveCmd  = MOVE_STOP;
	arrived  = 0;
	buzzer(0);
	doorLed(0);
	doorOpen = 0;
	state    = ST_ERROR;
	errText  = name;
	errFloor = floor;

	if (strcmp(name, "STOP") == 0) printf("EMERGENCY STOP  pos=%ld -> press any button to re-home\r\n", (long)position);
	else                           printf("ERROR(%s) %dF  pos=%ld -> press any button to re-home\r\n", name, floor, (long)position);
}

/* 층 호출 — 보드 버튼(fromPc = 0)과 PC "CALL n"(fromPc = 1)이 같이 씀 */
static void callFloor(int8_t floor, int fromPc)
{
	printf("CALL %dF%s\r\n", floor, fromPc ? " (PC)" : "");

	switch (state) {
	case ST_HOMING:
	case ST_MOVING:                                   // 움직이는 중
		if (fromPc) printf("BUSY -> ignored (use STOP)\r\n");
		else        stopWithError("STOP", curFloor);  // 보드 버튼 = 비상 정지
		break;

	case ST_ERROR:                                    // 멈춰 있던 중 → 원점부터 다시
		goHome();
		break;

	case ST_DOOR:                                     // 문 열린 중 → 기억했다가 닫히면 출발
		if (floor != curFloor) {
			nextFloor = floor;
			printf("queued %dF\r\n", floor);
		}
		break;

	case ST_IDLE:                                     // 서 있는 중
		if (floor == curFloor) openDoor();            // 지금 층 → 문만 열기
		else                   goToFloor(floor);      // 다른 층 → 출발
		break;
	}
}


/* ============================================================
 * 5. ap_init — 전원 켜면 한 번
 * ============================================================ */
void ap_init(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);   // printf 바로 출력

	/* 모터: 풀스텝(코일 2개씩 ON)으로 쓰려고 시작 패턴을 한 칸 맞춰 둠 */
	StepMotor_init(&motor, &motorPin);
	StepMotor_stepPhase(&motor, STEP_MOTOR_DIR_CW);

	/* LCD · 온습도 */
	lcdOk = LCD_init(&hi2c1);
	if (lcdOk) {
		LCD_printLine(0, "STM Elevator");
		LCD_printLine(1, "T: --.- H: --%");
	}
	TH_init();

	/* 타이머 · 부저 · 문 · UART 시작 */
	HAL_TIM_Base_Start_IT(&htim3);              // 1ms 인터럽트 → 모터
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);   // 부저 PWM
	buzzer(0);
	doorLed(0);
	HAL_UART_Receive_IT(&huart2, &rxByte, 1);   // PC 명령 받기 시작

	printf("\r\n[Elevator] buttons + PC (CALL n / STOP / HOME)\r\n");
	printf("LCD: %s\r\n", lcdOk ? "OK" : "NOT FOUND");
	printf("SENSOR  1F:%d  2F:%d  3F:%d  (1 = blocked)\r\n", isSensorBlocked(0), isSensorBlocked(1), isSensorBlocked(2));

	/* 시작 위치 확인 → 1층이 아니면 1층까지 내려가기 */
	if (countBlocked() >= 2) {
		printf("WARN: 2+ sensors blocked at start -> check wiring\r\n");
	}
	curFloor = findBlockedFloor();   // 1 · 2 · 3, 층 사이면 0

	if (curFloor == 1) {
		position = 0;
		state    = ST_IDLE;
		printf("START at 1F (home)\r\n");
	}
	else {
		printf("START at %dF (0 = between floors)\r\n", curFloor);
		goHome();
	}
}


/* ============================================================
 * 6. ap_main — while(1)에서 계속. 할 일 ①~⑦
 * ============================================================ */

/* ① PC 명령 처리: "CALL 1" ~ "CALL 3" · "STOP" · "HOME" (소문자도 됨) */
static void checkPcCommand(void)
{
	if (!cmdReady) return;

	char cmd[20];
	strcpy(cmd, cmdLine);   // 먼저 복사한 뒤 플래그를 내려야 다음 줄이 덮어쓰지 않음
	cmdReady = 0;

	for (int i = 0; cmd[i]; i++) {                       // 소문자 → 대문자
		if (cmd[i] >= 'a' && cmd[i] <= 'z') cmd[i] -= 'a' - 'A';
	}
	int n = strlen(cmd);
	while (n > 0 && cmd[n - 1] == ' ') cmd[--n] = '\0';  // 끝 공백 지우기

	if      (strcmp(cmd, "CALL 1") == 0) callFloor(1, 1);
	else if (strcmp(cmd, "CALL 2") == 0) callFloor(2, 1);
	else if (strcmp(cmd, "CALL 3") == 0) callFloor(3, 1);
	else if (strcmp(cmd, "STOP") == 0) {
		printf("STOP (PC)\r\n");
		if (state == ST_HOMING || state == ST_MOVING) stopWithError("STOP", curFloor);
	}
	else if (strcmp(cmd, "HOME") == 0) {
		printf("HOME (PC)\r\n");
		if (state == ST_IDLE || state == ST_ERROR) goHome();
		else                                       printf("BUSY -> ignored\r\n");
	}
	else {
		printf("? unknown command: %s\r\n", cmd);
	}
}

/* ② 타이머 인터럽트가 "목표 층에 멈췄다"(arrived = 1)고 알렸는지 확인 */
static void checkArrived(void)
{
	if (!arrived) return;
	arrived = 0;

	if (state == ST_HOMING) {                 // 원점 잡기 끝
		position = 0;
		curFloor = 1;
		state    = ST_IDLE;
		printf("HOME OK (1F)  pos=0\r\n");
	}
	else if (state == ST_MOVING) {
		if (moveSteps < TOO_FAST_STEPS) {     // 출발하자마자 도착 = 목표 센서가 계속 가려져 있던 것
			stopWithError("SENSOR", targetFloor);
			return;
		}
		printf("ARRIVED %dF  pos=%ld  steps=%lu\r\n", curFloor, (long)position, (unsigned long)moveSteps);
		openDoor();
	}
}

/* ③ 딩동 · 문: HAL_Delay 없이 "도착하고 몇 ms 지났나"로 판단 */
static void runDoor(uint32_t now)
{
	if (state != ST_DOOR) return;

	uint32_t t = now - doorTime;   // 도착하고 지난 시간

	/* 소리: 0 ~ 0.3초 딩, 0.3 ~ 0.9초 동, 그 뒤 조용 */
	if      (t < DING_MS)           buzzer(DING_HZ);
	else if (t < DING_MS + DONG_MS) buzzer(DONG_HZ);
	else                            buzzer(0);

	/* 2초: 문 열림 */
	if (t >= DOOR_OPEN_AT_MS && !doorOpen) {
		doorOpen = 1;
		doorLed(1);
		printf("DOOR OPEN  (%dF)\r\n", curFloor);
	}

	/* 5초: 문 닫힘 → 기억해 둔 층이 있으면 출발 */
	if (t >= DOOR_CLOSE_AT_MS) {
		doorOpen = 0;
		doorLed(0);
		state = ST_IDLE;
		printf("DOOR CLOSE (%dF)\r\n", curFloor);

		if (nextFloor) {
			int8_t f = nextFloor;
			nextFloor = 0;
			goToFloor(f);
		}
	}
}

/* ④ 버튼: 누르는 순간 한 번만 */
static void checkButtons(void)
{
	static int wasPressed[3] = { 0, 0, 0 };

	for (int i = 0; i < 3; i++) {
		int pressed = isButtonPressed(i);
		if (pressed && !wasPressed[i]) {
			callFloor(i + 1, 0);
		}
		wasPressed[i] = pressed;
	}
}

/* ⑤ 센서 · 이상 확인 */
static void checkSensors(void)
{
	/* 센서가 바뀔 때마다 로그 (배선 · 차광판 위치 확인용) */
	static int lastBlk[3] = { -1, -1, -1 };
	for (int i = 0; i < 3; i++) {
		int b = isSensorBlocked(i);
		if (b != lastBlk[i]) {
			lastBlk[i] = b;
			printf("  [%dF SENSOR %s]  pos=%ld\r\n", i + 1, b ? "BLOCKED" : "open", (long)position);
		}
	}

	/* 서 있을 때는 가려진 센서로 지금 층을 다시 확인 */
	if (moveCmd == MOVE_STOP && state != ST_ERROR) {
		int8_t f = findBlockedFloor();
		if (f) curFloor = f;
	}

	/* 센서 2개가 50ms 넘게 같이 가려짐 → 멈추지 않고 경고만 한 번 (고장 센서 하나로 전체가 서지 않게) */
	static uint8_t multiCount = 0;
	if (countBlocked() >= 2) {
		if (multiCount < 255) multiCount++;
		if (multiCount == 5) printf("WARN: 2+ sensors blocked at once -> check wiring\r\n");
	}
	else {
		multiCount = 0;
	}

	/* 타임아웃: 너무 오래 가도 목표 층 센서가 안 나옴 (탈조 · 센서 고장 · 배선 빠짐) */
	if (moveCmd != MOVE_STOP && moveSteps > TIMEOUT_STEPS) {
		stopWithError("TIMEOUT", targetFloor);
	}
}

/* ⑥ PC GUI로 상태 보내기
 *  $ST,<상태>,<지금 층>,<목표 층>,<방향 U/D/S>,<위치>,<문 0 닫힘 · 1 딩동 · 2 열림>,<센서 1F2F3F>,<에러>
 *  예) $ST,MOVING,2,3,U,1731,0,010,NONE
 *  값이 바뀌면 0.1초마다, 그대로여도 0.5초마다 한 번 */
static void sendStatus(uint32_t now)
{
	static const char *stateName[] = { "HOMING", "IDLE", "MOVING", "DOOR", "ERROR" };
	static uint32_t checkTime = 0, sendTime = 0;
	static char     lastMsg[64] = "";

	if (now - checkTime < 100) return;
	checkTime = now;

	int  door = (state != ST_DOOR) ? 0 : (doorOpen ? 2 : 1);
	char dir  = (moveCmd == MOVE_UP) ? 'U' : (moveCmd == MOVE_DOWN) ? 'D' : 'S';

	char msg[64];
	snprintf(msg, sizeof(msg), "$ST,%s,%d,%d,%c,%ld,%d,%d%d%d,%s",
	         stateName[state], curFloor, targetFloor, dir, (long)position, door,
	         isSensorBlocked(0), isSensorBlocked(1), isSensorBlocked(2), errText);

	if (strcmp(msg, lastMsg) != 0 || now - sendTime >= 500) {
		printf("%s\r\n", msg);
		strcpy(lastMsg, msg);
		sendTime = now;
	}
}

/* ⑦ LCD (0.1초마다, 글자가 바뀌었을 때만 다시 씀)
 *  1줄: FLOOR : 2F  ·  FLOOR:1F > 3F UP  ·  FLOOR : 2F OPEN  ·  HOMING 3F>1F  ·  STOPPED (BTN)  ·  ERR:TIMEOUT 3F
 *  2줄: T: 23.0 H: 28% */
static void updateLcd(uint32_t now)
{
	static uint32_t lastTime = 0;
	static char     shown0[32] = "", shown1[32] = "";   // 지금 LCD에 떠 있는 글자

	if (!lcdOk || now - lastTime < 100) return;
	lastTime = now;

	char line[32];
	char fl[3] = "--";                                  // 지금 층 글자 ("2F", 모르면 "--")
	if (curFloor) { fl[0] = '0' + curFloor; fl[1] = 'F'; }

	/* 1줄: 상태 */
	if      (state == ST_HOMING) snprintf(line, sizeof(line), "HOMING %s>1F", fl);
	else if (state == ST_MOVING) snprintf(line, sizeof(line), "FLOOR:%s > %dF %s", fl, targetFloor, (moveCmd == MOVE_UP) ? "UP" : "DN");
	else if (state == ST_ERROR)  {
		if (strcmp(errText, "STOP") == 0) snprintf(line, sizeof(line), "STOPPED (BTN)");
		else                              snprintf(line, sizeof(line), "ERR:%s %dF", errText, errFloor);
	}
	else if (doorOpen)           snprintf(line, sizeof(line), "FLOOR : %s OPEN", fl);
	else                         snprintf(line, sizeof(line), "FLOOR : %s", fl);

	if (strcmp(line, shown0) != 0) {
		LCD_printLine(0, line);
		strcpy(shown0, line);
	}

	/* 2줄: 온습도 */
	dht11_data_t d;
	if (TH_isValid()) {
		TH_getTempHumid(&d);
		snprintf(line, sizeof(line), "T: %2u.%u H: %2u%%", d.t_int, d.t_dec, d.h_int);
	}
	else {
		snprintf(line, sizeof(line), "T: --.- H: --%%");   // 아직 못 읽음
	}

	if (strcmp(line, shown1) != 0) {
		LCD_printLine(1, line);
		strcpy(shown1, line);
		if (TH_isValid()) printf("TEMP %u.%uC  HUMID %u%%\r\n", d.t_int, d.t_dec, d.h_int);
	}
}

void ap_main(void)
{
	static uint32_t lastPoll = 0;
	uint32_t now = HAL_GetTick();

	checkPcCommand();               // ① PC 명령
	checkArrived();                 // ② 목표 층 도착했나
	runDoor(now);                   // ③ 딩동 · 문

	if (now - lastPoll >= 10) {     // 10ms마다
		lastPoll = now;
		checkButtons();             // ④ 버튼
		checkSensors();             // ⑤ 센서 · 이상
	}

	if (moveCmd == MOVE_STOP) {     // 온습도는 멈춰 있을 때만 (DHT11은 타이밍이 민감해서
		TH_task();                  //   모터 인터럽트가 끼어들면 값이 깨짐)
	}

	sendStatus(now);                // ⑥ PC GUI
	updateLcd(now);                 // ⑦ LCD
}


/* ============================================================
 * 7. 인터럽트 (콜백은 프로젝트 전체에서 하나씩만 있어야 함)
 * ============================================================ */

/* 타이머 1ms → 2ms마다: 모터 1스텝 + 센서 확인 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance != TIM3) return;

	static uint8_t ms = 0;
	if (++ms < STEP_MS) return;          // 2ms에 한 번만 아래를 실행
	ms = 0;

	if (moveCmd == MOVE_STOP) return;    // 멈춤: 코일은 켜 둔 채로 (카가 안 미끄러짐)

	/* (1) 모터 1스텝: 하프스텝 2번 = 풀스텝 1번 (코일 2개가 항상 켜져 힘이 셈) */
	stepMotor_dir_t dir = (moveCmd == MOVE_UP) ? DIR_UP : DIR_DOWN;
	StepMotor_stepPhase(&motor, dir);
	StepMotor_stepPhase(&motor, dir);
	position += (moveCmd == MOVE_UP) ? 1 : -1;
	moveSteps++;

	/* (2) 센서 3개: 60스텝 "연속"으로 가려져야 그 층으로 인정
	 *     중간에 한 번이라도 열리면 0부터 다시 → 짧은 잡음 무시, 차광판이 센서 안쪽까지 들어간 뒤 정지 */
	for (int i = 0; i < 3; i++) {
		if (isSensorBlocked(i)) blkSteps[i]++;
		else                    blkSteps[i] = 0;

		if (blkSteps[i] == STOP_CONFIRM_STEPS) {
			curFloor = i + 1;                    // 지나가는 층도 지금 층으로
			if (curFloor == targetFloor) {
				moveCmd = MOVE_STOP;             // 목표 층 → 정지
				arrived = 1;                     // main에게 알림 → checkArrived()
			}
		}
	}
}

/* 센서 EXTI: 가려지는 순간 횟수만 셈 (확인용). 정지 판단은 위 타이머에서 함 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	for (int i = 0; i < 3; i++) {
		if (GPIO_Pin == senPin[i]) sensorHits[i]++;
	}
}

/* UART: 한 글자 받을 때마다 → 줄에 쌓고, 엔터('\r' 또는 '\n')면 한 줄 완성 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance != USART2) return;

	char c = (char)rxByte;
	if (c == '\r' || c == '\n') {
		if (rxLen > 0 && !cmdReady) {          // main이 앞 명령을 아직 안 읽었으면 이번 줄은 버림
			memcpy(cmdLine, rxBuf, rxLen);
			cmdLine[rxLen] = '\0';
			cmdReady = 1;
		}
		rxLen = 0;
	}
	else if (rxLen < sizeof(rxBuf) - 1) {
		rxBuf[rxLen++] = c;
	}

	HAL_UART_Receive_IT(&huart2, &rxByte, 1);  // 다음 한 글자 (다시 안 걸면 수신이 멈춤)
}

/* UART 에러(오버런 등)가 나면 HAL이 수신을 멈춤 → 다시 시작 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance != USART2) return;
	rxLen = 0;
	HAL_UART_Receive_IT(&huart2, &rxByte, 1);
}
