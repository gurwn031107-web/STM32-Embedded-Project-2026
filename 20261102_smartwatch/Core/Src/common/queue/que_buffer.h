/*
 * que_buffer.h
 *
 *  Created on: 2026. 10. 30.
 *      Author: kccistc
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef SRC_COMMON_QUEE_QUE_BUFFER_H_
#define SRC_COMMON_QUEE_QUE_BUFFER_H_

typedef struct {
	uint8_t *buf;		// 저장 공간 (ex 배열)
	uint16_t elem_size; // 요소 하나의 크기 (byte)
	uint16_t capacity;	// 배열 칸 수
	volatile uint16_t head;		// 다음에 넣을 칸
	volatile uint16_t tail; 		// 다음에 꺼낼 칸
} que_buf_t;

void que_buf_init(que_buf_t *que, void *storage, uint16_t elem_size, uint16_t capacity);
bool que_put(que_buf_t *que, void *elem);
bool que_get(que_buf_t *que, void *elem);
bool que_is_empty(que_buf_t *que);
bool que_is_full(que_buf_t *que);

#endif /* SRC_COMMON_QUEE_QUE_BUFFER_H_ */
