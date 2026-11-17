/*
 * comm_task.c
 *
 *  Created on: 2026. 11. 12.
 *      Author: kccistc
 */


#include "comm_task.h"
#include "usart.h"
#include "../../message.h"
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

void comm_task_init(void)
{
	HAL_UART_Receive_IT(&huart2, &rx_data, 1);
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
		else {
			osMessagePut(commQueueHandle, COMM_NO, 10);
			if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
				Lcd_writeStringXY(1, 0, "Comm Task NO");
				osMutexRelease(lcdMutexHandle);
			}
		}
		rx_index = 0;
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
}
