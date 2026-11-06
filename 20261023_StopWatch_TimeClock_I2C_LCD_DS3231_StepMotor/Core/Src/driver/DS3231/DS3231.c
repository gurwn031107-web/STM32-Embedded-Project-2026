#include "ds3231.h"
#define DS3231_ADDR (0x68U << 1)
#define RTC_TIMEOUT 10U
static I2C_HandleTypeDef *rtcI2c;
static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10U + (v & 15U); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10U) << 4) | (v % 10U); }
static uint8_t decodeHour(uint8_t v)
{
    if (v & 0x40U) return bcd2bin(v & 0x1fU) % 12U + ((v & 0x20U) ? 12U : 0U);
    return bcd2bin(v & 0x3fU);
}
void ds3231_init(I2C_HandleTypeDef *hi2c) { rtcI2c = hi2c; }
void ds3231_getDateTime(ds3231_datetime_t *dt)
{
    uint8_t b[7];
    if (!rtcI2c || !dt) return;
    if (HAL_I2C_Mem_Read(rtcI2c, DS3231_ADDR, 0, I2C_MEMADD_SIZE_8BIT,
                         b, sizeof b, RTC_TIMEOUT) != HAL_OK) return;
    ds3231_datetime_t next = {
        {bcd2bin(b[0] & 0x7fU), bcd2bin(b[1] & 0x7fU), decodeHour(b[2])},
        {bcd2bin(b[3] & 7U), bcd2bin(b[4] & 0x3fU), bcd2bin(b[5] & 0x1fU), bcd2bin(b[6])}
    };
    if (next.time.sec > 59 || next.time.min > 59 || next.time.hour > 23 ||
        next.date.day < 1 || next.date.day > 7 || next.date.date < 1 ||
        next.date.date > 31 || next.date.month < 1 || next.date.month > 12 ||
        next.date.year > 99) return;
    *dt = next;
}
void ds3231_getTime(ds3231_time_t *t)
{
    ds3231_datetime_t dt = {0};
    if (!t) return;
    ds3231_getDateTime(&dt);
    if (dt.date.day) *t = dt.time;
}
void ds3231_getDate(ds3231_date_t *d)
{
    ds3231_datetime_t dt = {0};
    if (!d) return;
    ds3231_getDateTime(&dt);
    if (dt.date.day) *d = dt.date;
}
void ds3231_setTime(const ds3231_time_t *t)
{
    if (!rtcI2c || !t) return;
    uint8_t b[] = {bin2bcd(t->sec), bin2bcd(t->min), bin2bcd(t->hour)};
    HAL_I2C_Mem_Write(rtcI2c, DS3231_ADDR, 0, I2C_MEMADD_SIZE_8BIT, b, sizeof b, RTC_TIMEOUT);
}
void ds3231_setDate(const ds3231_date_t *d)
{
    if (!rtcI2c || !d) return;
    uint8_t b[] = {bin2bcd(d->day), bin2bcd(d->date), bin2bcd(d->month), bin2bcd(d->year)};
    HAL_I2C_Mem_Write(rtcI2c, DS3231_ADDR, 3, I2C_MEMADD_SIZE_8BIT, b, sizeof b, RTC_TIMEOUT);
}
void ds3231_setDateTime(const ds3231_datetime_t *dt)
{
    if (!rtcI2c || !dt) return;
    uint8_t b[] = {bin2bcd(dt->time.sec), bin2bcd(dt->time.min), bin2bcd(dt->time.hour),
                  bin2bcd(dt->date.day), bin2bcd(dt->date.date), bin2bcd(dt->date.month),
                  bin2bcd(dt->date.year)};
    HAL_I2C_Mem_Write(rtcI2c, DS3231_ADDR, 0, I2C_MEMADD_SIZE_8BIT, b, sizeof b, RTC_TIMEOUT);
}
