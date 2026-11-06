#include "TimeClock.h"
#include "../../driver/ds3231/ds3231.h"

extern I2C_HandleTypeDef hi2c1;
/* Failed reads preserve the last valid value. Never reset RTC at boot. */
static ds3231_datetime_t rtc = {{0, 0, 0}, {1, 1, 1, 0}};
static volatile uint8_t centisecond;
static uint32_t lastRead;
void TimeClock_init(void)
{
    ds3231_init(&hi2c1);
    ds3231_getDateTime(&rtc);
    centisecond = 0;
    lastRead = HAL_GetTick();
}
TimeClock_t *TimeClock_getTimeData(void)
{
    static TimeClock_t snapshot;
    snapshot = (TimeClock_t){rtc.time.hour, rtc.time.min, rtc.time.sec, centisecond};
    return &snapshot;
}
TimeDate_t *TimeClock_getDateData(void)
{
    static TimeDate_t snapshot;
    snapshot = (TimeDate_t){rtc.date.day, rtc.date.date, rtc.date.month, rtc.date.year};
    return &snapshot;
}
void TimeClock_ISR(void) { centisecond = (centisecond + 1U) % 100U; }
void TimeClock_task(void)
{
    if ((uint32_t)(HAL_GetTick() - lastRead) < 100U) return;
    lastRead = HAL_GetTick();
    ds3231_getDateTime(&rtc);
}
