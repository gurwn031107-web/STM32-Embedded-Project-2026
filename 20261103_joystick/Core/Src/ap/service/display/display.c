#include "display.h"
#include "../joystickCounter/joystickCounter.h"
#include "../../../driver/lcd/lcd.h"
#include <stdio.h>
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c1;
static uint32_t last_display;
static int shown_x, shown_y;
static bool shown_error;

static void display_write(void)
{
    char line[17];
    shown_x = joystickCounter_getX();
    shown_y = joystickCounter_getY();
    shown_error = joystickCounter_hasError();
    if (shown_error) {
        Lcd_writeStringXY(0, 0, "ADC ERROR       ");
        Lcd_writeStringXY(1, 0, "Check ADC1      ");
        return;
    }
    (void)snprintf(line, sizeof(line), "X:%+05d         ", shown_x);
    Lcd_writeStringXY(0, 0, line);
    (void)snprintf(line, sizeof(line), "Y:%+05d         ", shown_y);
    Lcd_writeStringXY(1, 0, line);
}

void display_init(void)
{
    /* main.c must initialize I2C1 and SysTick before this call. */
    Lcd_init(&hi2c1);
    display_write();
    last_display = HAL_GetTick();
}

void display_task(void)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_display) < 50U) return;
    if (shown_x == joystickCounter_getX() &&
        shown_y == joystickCounter_getY() &&
        shown_error == joystickCounter_hasError()) return;
    display_write();
    last_display = HAL_GetTick();
}
