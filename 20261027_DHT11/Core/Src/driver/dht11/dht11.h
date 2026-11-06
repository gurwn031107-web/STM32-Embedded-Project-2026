/*
 * dht11.h
 *
 *  Created on: 2026. 10. 27.
 *      Author: kccistc
 */

#ifndef SRC_DRIVER_DHT11_H_
#define SRC_DRIVER_DHT11_H_

#include "stm32f4xx_hal.h"

typedef enum {
	DHT11_OK = 0,
	DHT11_ERR_NO_RESPONSE,
	DHT11_ERR_TIMEOUT,
	DHT11_ERR_CHECKSUM
} dht11_status_t;

typedef struct {
	uint8_t h_int;
	uint8_t h_dec;
	uint8_t t_int;
	uint8_t t_dec;
} dht11_data_t;


dht11_status_t dht11_read(dht11_data_t *d);
void dht11_init(GPIO_TypeDef *gpio, uint16_t pin);

#endif /* SRC_DRIVER_DHT11_H_ */
