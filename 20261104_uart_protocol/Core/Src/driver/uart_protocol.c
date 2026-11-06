/*
 * uart_protocol.c
 *
 *  Created on: 2026. 11. 4.
 *      Author: kccistc
 */


#include "uart_protocol.h"

typedef enum {
	RX_WAIT_START,
	RX_WAIT_LEN,
	RX_WAIT_DATA,
	RX_WAIT_CHK
} rx_state_t;

void packet_do_func(uint8_t cmd, uint8_t *payload, uint8_t payload_len);

static volatile uint8_t rx_byte;
static rx_state_t rx_state = RX_WAIT_START;
static uint8_t rx_len;
static uint8_t rx_idx;
static uint8_t rx_buf[PKT_MAX_LEN];

UART_HandleTypeDef *protocol_huart;

// 함수 포인터 변수 선언, 함수의 주소를 넣을 수 있는 공간, 함수 주소를 받은 함수를 대신하는 함수
static uart_packet_callback_t packet_callback = 0;

void UART_protocol_init(UART_HandleTypeDef *huart, uart_packet_callback_t packet_cb)
{
	protocol_huart = huart;
	packet_callback = packet_cb;
	rx_state = RX_WAIT_START;
	HAL_UART_Receive_IT(protocol_huart, (uint8_t *)&rx_byte, 1);
}

static void rx_parse_byte(uint8_t rx_data)
{
	switch (rx_state)
	{
	case RX_WAIT_START:
		if (rx_data == PKT_STATE) {
			rx_state = RX_WAIT_LEN;
		}
		break;
	case RX_WAIT_LEN:
		if (rx_data == 0 || rx_data >= PKT_MAX_LEN) {
			rx_state = RX_WAIT_START;
		}
		else {
			rx_len = rx_data;
			rx_idx = 0;
			rx_state = RX_WAIT_DATA;
		}
		break;
	case RX_WAIT_DATA:
		rx_buf[rx_idx++] = rx_data;
		if (rx_idx >= rx_len) {
			rx_state = RX_WAIT_CHK;
		}
		break;
	case RX_WAIT_CHK:
		uint8_t cksum = rx_len;
		for (int i=0; i<rx_len; i++) {
			cksum += rx_buf[i];
		}
		if (cksum == rx_data) {
			// 수신한 명령 내용 함수 실행 -> ok 전송
			//packet_do_func(rx_buf[0], &rx_buf[1], (rx_len-1));
			packet_callback(rx_buf[0], &rx_buf[1], (rx_len-1));
		}
		else {
			// error 전송
		}
		rx_state = RX_WAIT_START;
		break;
	default:
		break;

	}
}

void UART_protocol_send(uint8_t cmd, uint8_t *payload, uint8_t payload_len)
{
	uint8_t tx[3+PKT_MAX_LEN];
	uint8_t len = payload_len + 1;
	uint8_t cksum = len;

	tx[0] = PKT_STATE;
	tx[1] = len;
	tx[2] = cmd;
	cksum += cmd;

	for (int i=0; i<payload_len; i++) {
		tx[3+i] = payload[i];
		cksum += payload[i];
	}
	tx[3+payload_len] = cksum;

	HAL_UART_Transmit(protocol_huart, tx, (3 + payload_len + 1), 1000);
}
/*
void packet_do_func(uint8_t cmd, uint8_t *payload, uint8_t payload_len)
{
	switch (cmd)
	{
	case CMD_LED0_SET: {
		uint8_t send_payload;
		if (payload_len >= 1) {
			if (payload[0] == 1) {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
			}
			send_payload = RSP_OK;
		}
		else {
			send_payload = RSP_ERR;
		}
		UART_protocol_send(cmd+0x80, &send_payload, 1);
		break;
	}
	case CMD_LED1_SET: {
		uint8_t send_payload;
		if (payload_len >= 1) {
			if (payload[0] == 1) {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
			}
			send_payload = RSP_OK;
		}
		else {
			send_payload = RSP_ERR;
		}
		//		UART_protocol_send(CMD_LED_RSP, &send_payload, 1);
		UART_protocol_send(cmd+0x80, &send_payload, 1);
		break;
	}
	case CMD_LED2_SET: {
		uint8_t send_payload;
		if (payload_len >= 1) {
			if (payload[0] == 1) {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
			}
			send_payload = RSP_OK;
		}
		else {
			send_payload = RSP_ERR;
		}
		//UART_protocol_send(CMD_LED_RSP, &send_payload, 1);
		UART_protocol_send(cmd+0x80, &send_payload, 1);
		break;
	}
	case CMD_LED3_SET: {
		uint8_t send_payload;
		if (payload_len >= 1) {
			if (payload[0] == 1) {
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
			}
			send_payload = RSP_OK;
		}
		else {
			send_payload = RSP_ERR;
		}
		//UART_protocol_send(CMD_LED_RSP, &send_payload, 1);
		UART_protocol_send(cmd+0x80, &send_payload, 1);
		break;
	}
	case CMD_LED_STATUS_REQ: {
		uint8_t send_payload[4];
		//		for (int i=0; i<4; i++) {
		//			send_payload[i] = HAL_GPIO_ReadPin(led[i].port, led[i].pin) == GPIO_PIN_SET ? 1 : 0;
		//		}
		send_payload[0] = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15) == GPIO_PIN_SET ? 1 : 0);
		send_payload[1] = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET ? 1 : 0);
		send_payload[2] = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_SET ? 1 : 0);
		send_payload[3] = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_SET ? 1 : 0);
		UART_protocol_send(cmd+0x80, send_payload, 4);
		break;
	}
	case CMD_TEMP_HUMID_REQ: {
		uint8_t dummy_temp_int = 21;
		uint8_t dummy_temp_dec = 1;
		uint8_t dummy_humid_int = 48;
		uint8_t dummy_humid_dec = 0;

		uint8_t send_payload[4];
		send_payload[0] = dummy_temp_int;
		send_payload[1] = dummy_temp_dec;
		send_payload[2] = dummy_humid_int;
		send_payload[3] = dummy_humid_dec;

		UART_protocol_send(cmd+0x80, send_payload, 4);
		break;
	}
	case CMD_DISTANCE_REQ:
		uint16_t dummy_distance = 0x01fa;  //287;
		uint8_t send_payload[2];
		send_payload[0] = (uint8_t)((uint16_t)dummy_distance >> 8);
		send_payload[1] = (uint8_t)((uint16_t)dummy_distance & 0xff);

		UART_protocol_send(cmd+0x80, send_payload, 2);
		break;
	default:
		break;

	}
}
*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance = USART2) {
		rx_parse_byte(rx_byte);
		HAL_UART_Receive_IT(protocol_huart, (uint8_t *)&rx_byte, 1);
	}
}
