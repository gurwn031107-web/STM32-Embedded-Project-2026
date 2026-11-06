#include "Display.h"
#include "../../driver/fnd/fnd.h"
#include "../../driver/led/led.h"
#include "../../driver/lcd/lcd.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
static led_t ledClock, ledSW, ledRun, ledStop;
static char previous[2][17];
static uint8_t valid[2];
/* Exactly 16 columns, and only changed characters are transmitted. */
static void writeLine(uint8_t row, const char *text)
{
    char line[17];
    memset(line, ' ', 16);
    line[16] = '\0';
    for (int i = 0; i < 16 && text[i]; ++i) line[i] = text[i];
    int col = 0;
    while (col < 16) {
        if (valid[row] && line[col] == previous[row][col]) { ++col; continue; }
        Lcd_gotoXY(row, col);
        do {
            Lcd_writeCharData(line[col]);
            ++col;
        } while (col < 16 && (!valid[row] || line[col] != previous[row][col]));
    }
    memcpy(previous[row], line, 17);
    valid[row] = 1;
}
void Disp_init(void)
{
    led_init(&ledClock, GPIOB, GPIO_PIN_15);
    led_init(&ledSW, GPIOB, GPIO_PIN_14);
    led_init(&ledRun, GPIOC, GPIO_PIN_4);
    led_init(&ledStop, GPIOB, GPIO_PIN_13);
    valid[0] = valid[1] = 0;
    Lcd_init(&hi2c1);
}
void Disp_setMode(int mode)
{
    if (mode == 0) { led_on(&ledClock); led_off(&ledSW); }
    else { led_off(&ledClock); led_on(&ledSW); }
}
void Disp_setStopWatchState(int state)
{
    if (state == RUN) { led_on(&ledRun); led_off(&ledStop); }
    else { led_off(&ledRun); led_on(&ledStop); }
}
void Disp_setLcdTimeClockTime(timeClockTime_t *t)
{
    char line[32];
    snprintf(line, sizeof line, "TIME:%02u:%02u:%02u",
             (unsigned)t->hour, (unsigned)t->min, (unsigned)t->sec);
    writeLine(0, line);
}
void Disp_setTimeClockTime(timeClockTime_t *t)
{
    fnd_set_num(t->min * 100U + t->sec);
    fnd_set_dp(FND_DIGIT_1, FND_DP_OFF);
    fnd_set_dp(FND_DIGIT_2, t->msec < 50 ? FND_DP_ON : FND_DP_OFF);
    fnd_set_dp(FND_DIGIT_3, FND_DP_OFF);
    fnd_set_dp(FND_DIGIT_4, FND_DP_OFF);
    Disp_setLcdTimeClockTime(t);
}
void Disp_setStopWatchTime(stopWatchTime_t *t)
{
    fnd_set_num((t->min % 10U) * 1000U + t->sec * 10U + t->msec / 10U);
    fnd_set_dp(FND_DIGIT_1, FND_DP_ON);
    fnd_set_dp(FND_DIGIT_2, FND_DP_OFF);
    fnd_set_dp(FND_DIGIT_3, t->msec < 50 ? FND_DP_ON : FND_DP_OFF);
    fnd_set_dp(FND_DIGIT_4, FND_DP_OFF);
    char line[32];
    snprintf(line, sizeof line, "SW  %02u:%02u:%02u.%02u",
             (unsigned)t->hour, (unsigned)t->min, (unsigned)t->sec, (unsigned)t->msec);
    writeLine(1, line);
}
void Disp_setTimeClockDate(timeClockDate_t *d)
{
    static const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char *day = d->day >= 1 && d->day <= 7 ? days[d->day - 1] : "---";
    char line[32];
    snprintf(line, sizeof line, "20%02u-%02u-%02u [%s]",
             (unsigned)d->year, (unsigned)d->month, (unsigned)d->date, day);
    writeLine(1, line);
}
void Disp_ISR(void) { fnd_show_number(); }
