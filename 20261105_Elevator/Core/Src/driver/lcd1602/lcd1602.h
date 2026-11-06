/*
 * lcd1602.h
 *  I2C LCD 1602 (PCF8574 백팩), 4bit 모드
 *  PCF8574 핀: P0=RS, P1=RW, P2=E, P3=백라이트, P4~P7=D4~D7
 */

#ifndef DRIVER_LCD1602_LCD1602_H_
#define DRIVER_LCD1602_LCD1602_H_

#include "main.h"

uint8_t LCD_init(I2C_HandleTypeDef *hi2c);               // 1: 성공, 0: LCD 못 찾음 (0x27 → 0x3F 순서로 찾음)
void    LCD_clear(void);
void    LCD_setCursor(uint8_t row, uint8_t col);         // row 0 ~ 1, col 0 ~ 15
void    LCD_print(const char *str);
void    LCD_printLine(uint8_t row, const char *str);     // 한 줄을 16칸으로 채워서 출력 (이전 글자 안 남음)

#endif /* DRIVER_LCD1602_LCD1602_H_ */
