/*
 * dht11.c
 *
 *  Created on: 2026. 10. 27.
 *      Author: kccistc
 */

#include "dht11.h"
#include "../../common/delay.h"

#define RESPONSE_TIMEOUT_US		100
#define BIT_ONE_US				40


static GPIO_TypeDef *dht11_gpio;
static uint16_t dht11_pin;

void dht11_init(GPIO_TypeDef *gpio, uint16_t pin)
{
	dht11_gpio = gpio;
	dht11_pin = pin;
	delay_init();
	HAL_GPIO_WritePin(dht11_gpio, dht11_pin, GPIO_PIN_SET);

}

static void pin_set_output()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin : PC8 */
	GPIO_InitStruct.Pin = dht11_pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(dht11_gpio, &GPIO_InitStruct);
}

static void pin_set_input()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin : PC8 */
	GPIO_InitStruct.Pin = dht11_pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(dht11_gpio, &GPIO_InitStruct);
}

static GPIO_PinState line_read(void)
{
	return HAL_GPIO_ReadPin(dht11_gpio, dht11_pin);
}

static int32_t wait_while (GPIO_PinState level, uint32_t timeout_us)
{
	uint32_t start = delay_get_cycle();

	while(line_read() == level) {
		if (delay_elapsed_us(start) > timeout_us) {
			return -1;
		}
	}
	return (int32_t)delay_elapsed_us(start);
}

static dht11_status_t dht11_readFrame(uint8_t *buf)
{
	if (wait_while(GPIO_PIN_SET, RESPONSE_TIMEOUT_US) < 0) {
		return DHT11_ERR_NO_RESPONSE;
	}

	// Low 80us, HIGH 80us
	if (wait_while(GPIO_PIN_RESET, RESPONSE_TIMEOUT_US) < 0) {
		return DHT11_ERR_NO_RESPONSE;
	}
	if (wait_while(GPIO_PIN_SET, RESPONSE_TIMEOUT_US) < 0) {
		return DHT11_ERR_NO_RESPONSE;
	}
	// Low 상태
	// bit 40개: MSB 부터
	for (int i = 0; i < 40; i++) {
		if (wait_while(GPIO_PIN_RESET, RESPONSE_TIMEOUT_US) < 0) {
			return DHT11_ERR_NO_RESPONSE;
		}
		int32_t high_us = wait_while(GPIO_PIN_SET, RESPONSE_TIMEOUT_US);
		if (high_us < 0) {
			return DHT11_ERR_NO_RESPONSE;
		}
		uint8_t bit = (high_us > BIT_ONE_US) ? 1 : 0;
		buf[i/8] = (uint8_t)((buf[i/8] << 1) | bit);
	}
	return DHT11_OK;
}

dht11_status_t dht11_read(dht11_data_t *d)
{
	uint8_t buf[5] = {0};

	pin_set_output(); // pin low
	HAL_GPIO_WritePin(dht11_gpio, dht11_pin, GPIO_PIN_RESET);
	HAL_Delay(18);
	HAL_GPIO_WritePin(dht11_gpio, dht11_pin, GPIO_PIN_SET);
	pin_set_input();

	dht11_status_t s = dht11_readFrame(buf); // dht11 습도, 온도, checksum

	if (s != DHT11_OK) {
		return s;
	}
	uint8_t checksum = (buf[0] + buf[1] + buf[2] + buf[3]);
	if (checksum != buf[4]){
		return DHT11_ERR_CHECKSUM;
		//		printf("checksum error\n");
	}

	d->h_int 	= buf[0];
	d->h_dec 	= buf[1];
	d->t_int 	= buf[2];
	d->t_dec 	= buf[3];

	return DHT11_OK;
}

