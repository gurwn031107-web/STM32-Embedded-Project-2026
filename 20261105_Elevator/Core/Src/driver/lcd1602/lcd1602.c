/*
 * lcd1602.c
 *  I2C LCD 1602 (PCF8574 백팩), 4bit 모드
 *  main loop에서만 호출 (I2C · HAL_Delay 사용 → ISR 안에서 쓰지 않기)
 */

#include "lcd1602.h"

#define LCD_RS   0x01
#define LCD_RW   0x02
#define LCD_E    0x04
#define LCD_BL   0x08   // 백라이트 ON

static I2C_HandleTypeDef *lcd_hi2c;
static uint16_t lcd_addr = (0x27 << 1);   // HAL은 8bit 주소 (7bit << 1)

/* 4bit 하나 보내기: 데이터를 먼저 놓고 E를 HIGH → LOW (떨어지는 순간 LCD가 읽음) */
static void LCD_writeNibble(uint8_t nibble, uint8_t rs)
{
	uint8_t data = (uint8_t)((nibble << 4) | LCD_BL | (rs ? LCD_RS : 0));
	uint8_t buf[3] = { data, (uint8_t)(data | LCD_E), data };
	HAL_I2C_Master_Transmit(lcd_hi2c, lcd_addr, buf, 3, 10);
}

/* 8bit = 상위 4bit → 하위 4bit */
static void LCD_send(uint8_t value, uint8_t rs)
{
	LCD_writeNibble(value >> 4, rs);
	LCD_writeNibble(value & 0x0F, rs);
}

static void LCD_cmd(uint8_t cmd)
{
	LCD_send(cmd, 0);
	if (cmd == 0x01 || cmd == 0x02) {
		HAL_Delay(2);   // clear · home은 1.5ms 이상 걸림
	}
}

uint8_t LCD_init(I2C_HandleTypeDef *hi2c)
{
	lcd_hi2c = hi2c;

	if (HAL_I2C_IsDeviceReady(hi2c, 0x27 << 1, 3, 10) == HAL_OK) {
		lcd_addr = 0x27 << 1;
	}
	else if (HAL_I2C_IsDeviceReady(hi2c, 0x3F << 1, 3, 10) == HAL_OK) {
		lcd_addr = 0x3F << 1;
	}
	else {
		return 0;
	}

	/* 4bit 모드 진입 순서 (HD44780 데이터시트) */
	HAL_Delay(50);
	LCD_writeNibble(0x03, 0);  HAL_Delay(5);
	LCD_writeNibble(0x03, 0);  HAL_Delay(1);
	LCD_writeNibble(0x03, 0);  HAL_Delay(1);
	LCD_writeNibble(0x02, 0);  HAL_Delay(1);

	LCD_cmd(0x28);   // 4bit, 2줄, 5x8
	LCD_cmd(0x08);   // 화면 OFF
	LCD_cmd(0x01);   // 화면 지우기
	LCD_cmd(0x06);   // 글자 쓰면 커서 오른쪽으로
	LCD_cmd(0x0C);   // 화면 ON, 커서 안 보이게
	return 1;
}

void LCD_clear(void)
{
	LCD_cmd(0x01);
}

void LCD_setCursor(uint8_t row, uint8_t col)
{
	LCD_cmd((uint8_t)(0x80 | ((row ? 0x40 : 0x00) + col)));
}

void LCD_print(const char *str)
{
	while (*str) {
		LCD_send((uint8_t)*str++, 1);
	}
}

void LCD_printLine(uint8_t row, const char *str)
{
	char buf[17];
	uint8_t i = 0;

	for (; i < 16 && str[i]; i++) buf[i] = str[i];
	for (; i < 16; i++)           buf[i] = ' ';    // 남는 칸은 공백 → 이전 글자 지움
	buf[16] = '\0';

	LCD_setCursor(row, 0);
	LCD_print(buf);
}
