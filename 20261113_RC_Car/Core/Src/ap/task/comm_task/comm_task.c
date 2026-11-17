/*
 * comm_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */


#include "comm_task.h"
#include "usart.h"
#include "../../message.h"
#include "../../auto_message.h"   // [자동 구조 추가] drive_mode_set()
#include "../../../driver/lcd/lcd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//extern UART_HandleTypeDef huart2; // usart.h에 들어있음.

static comm_data_t comm_data;
static uint8_t rx_data = 0;
static uint8_t rx_index = 0;
static uint8_t rx_cplt_flag = 0;

static char rx_buff[32];

/* ===== [자동 구조 추가] 블루투스 (USART6) ===== */
#define BT_ENABLE   0   // 0: 블루투스 수신 끔 (지금은 자동 주행만 테스트)
                        // 1: 휴대폰 'A'(자동 시작) / 'S'(정지) 사용
                        // 모듈을 안 꽂은 상태로 켜 두면 RX 핀에 노이즈가 들어와
                        // 엉뚱한 글자로 인식될 수 있어서 기본은 꺼둠
static uint8_t bt_rx_data = 0;   // 블루투스로 받은 1글자 (인터럽트가 채움)
static uint8_t bt_rx_cmd = 0;    // comm_task()에서 처리할 명령 글자
static uint8_t bt_rx_flag = 0;   // 1: 새 명령 도착

// 휴대폰으로 응답 보내기 (명령이 잘 들어갔는지 앱 화면에서 확인용)
static void bt_send(char *str)
{
	HAL_UART_Transmit(&huart6, (uint8_t *)str, strlen(str), 100);
}

// 블루투스 명령 처리: 1글자 = 1명령
//   'A' : 자동 주행 시작
//   'S' : 정지 (자동이면 수동 모드로 돌아가며 정지, 수동이면 STOP)
static void bt_command(uint8_t cmd)
{
	switch (cmd)
	{
	case 'A':
	case 'a':
		drive_mode_set(MODE_AUTO);
		bt_send("AUTO START\r\n");
		if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
			Lcd_writeStringXY(1, 0, "BT AUTO START   ");
			osMutexRelease(lcdMutexHandle);
		}
		break;

	case 'S':
	case 's':
		drive_mode_set(MODE_MANUAL);           // 자동 모드였다면 → 수동 + 정지
		comm_data.status = COMM_STOP;          // 수동 모드였다면 → 기존 STOP 명령과 똑같이 처리
		osMessagePut(commQueueHandle, (uint32_t)&comm_data, 10);
		bt_send("STOP\r\n");
		if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
			Lcd_writeStringXY(1, 0, "BT STOP         ");
			osMutexRelease(lcdMutexHandle);
		}
		break;

	default:   // 그 외 글자(앱이 붙여 보내는 줄바꿈 \r \n 등)는 무시
		break;
	}
}
/* ===== [자동 구조 추가] 끝 ===== */

void comm_task_init(void)
{
	HAL_UART_Receive_IT(&huart2, &rx_data, 1);
#if BT_ENABLE
	HAL_UART_Receive_IT(&huart6, &bt_rx_data, 1);   // [자동 구조 추가] 블루투스 수신 시작
#endif
}

void comm_task(void)
{
	if (rx_cplt_flag) {
		rx_cplt_flag = 0;
		if (strncmp(rx_buff, "RUN", 3) == 0) { // 같으면 0, 다르면 0이 아닌 값 (보통 다르면 1 또는 -1이 리턴 됨)
			comm_data.status = COMM_RUN;
//			osMessagePut(commQueueHandle, COMM_RUN, 10);
			osMessagePut(commQueueHandle, (uint32_t)&comm_data, 10);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task RUN       ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else if (strncmp(rx_buff, "STOP", 4) == 0) {
			comm_data.status = COMM_STOP;
//			osMessagePut(commQueueHandle, COMM_STOP, 10);
			osMessagePut(commQueueHandle, (uint32_t)&comm_data, 10);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task STOP      ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else if (strncmp(rx_buff, "SPD", 3) == 0) {
			comm_data.status = COMM_SPEED;
			comm_data.speed = atoi(&rx_buff[3]); // Ascii 코드가 숫자로 변환되어서 받음.

//			osMessagePut(commQueueHandle, COMM_STOP, 10);
			osMessagePut(commQueueHandle, (uint32_t)&comm_data, 10);

			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				char buf[30];
				sprintf(buf, "Comm SPD %d          ", comm_data.speed);
				Lcd_writeStringXY(1, 0, buf);
				osMutexRelease(lcdMutexHandle);
			}
		}
		/* ===== [자동 구조 추가] 모드 전환 명령 ===== */
		else if (strncmp(rx_buff, "AUTO", 4) == 0) {
			drive_mode_set(MODE_AUTO);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task AUTO      ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		else if (strncmp(rx_buff, "MANUAL", 6) == 0) {
			drive_mode_set(MODE_MANUAL);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task MANUAL    ");
				osMutexRelease(lcdMutexHandle);
			}
		}
		/* ===== [자동 구조 추가] 끝 ===== */
		else {
			osMessagePut(commQueueHandle, COMM_NO, 10);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task NO");
				osMutexRelease(lcdMutexHandle);
			}
		}
		rx_index = 0;
	}

	/* ===== [자동 구조 추가] 블루투스 명령 처리 ===== */
	if (bt_rx_flag) {
		bt_rx_flag = 0;
		bt_command(bt_rx_cmd);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2) {
		if (rx_data == '*') {
//			rx_buff[rx_index++] = 0; // 0 null
			rx_buff[rx_index++] = '\0'; // '\0' null 문자
			rx_cplt_flag = 1;
		}
		else {
			rx_buff[rx_index++] = rx_data;

		}

		HAL_UART_Receive_IT(&huart2, &rx_data, 1);
	}
	/* ===== [자동 구조 추가] 블루투스: 1글자 받으면 플래그만 세우고 바로 다음 수신 ===== */
	else if (huart->Instance == USART6) {
		bt_rx_cmd = bt_rx_data;
		bt_rx_flag = 1;
		HAL_UART_Receive_IT(&huart6, &bt_rx_data, 1);
	}
}

/* ===== [자동 구조 추가] 블루투스 수신 에러 복구 =====
 * 전원 켤 때 노이즈 등으로 수신 에러(Overrun)가 나면 HAL이 수신을 멈춘다.
 * 그러면 휴대폰 명령이 안 먹히므로, 에러가 나면 수신을 다시 시작한다.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART6) {
		HAL_UART_Receive_IT(&huart6, &bt_rx_data, 1);
	}
}
