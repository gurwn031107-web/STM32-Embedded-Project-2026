#include "joystickCounter.h"
#include "../../../driver/joystick/joystick.h"
#include "stm32f4xx_hal.h"

/* 조이스틱 핀 헤더가 위를 향하도록 설치
 *   좌우   -> VRy(PB0) -> X좌표 : 오른쪽으로 밀면 VRy 감소 -> -1
 *   위아래 -> VRx(PA4) -> Y좌표 : 위로 밀면 VRx 감소     -> -1
 * 방향이 반대인 축만 부호를 바꾸면 된다. */
#define JOY_X_POLARITY -1
#define JOY_Y_POLARITY -1
#define JOY_CENTER 2048
#define JOY_DEADZONE 450
#define JOY_LIMIT 1000
#define JOY_SAMPLE_MS 10U

typedef struct {
    int value;
    int direction;
    uint32_t last_step;
} joy_axis_t;
static joy_axis_t x_axis, y_axis;
static uint32_t last_sample;
static bool initialized, adc_error;

static bool axis_update(joy_axis_t *axis, uint16_t raw, int polarity,
                        uint32_t now)
{
    int delta = ((int)raw - JOY_CENTER) * polarity;
    int magnitude = delta < 0 ? -delta : delta;
    int direction = magnitude <= JOY_DEADZONE ? 0 : (delta > 0 ? 1 : -1);
    uint32_t interval = magnitude > 1500 ? 70U : magnitude > 900 ? 180U : 400U;
    if (direction == 0) {
        axis->direction = 0;
        axis->last_step = now;
        return false;
    }
    /* First movement/reversal: one immediate step. Held: repeat by time. */
    if (direction == axis->direction &&
        (uint32_t)(now - axis->last_step) < interval) return false;
    axis->direction = direction;
    axis->last_step = now;
    int next = axis->value + direction;
    if (next > JOY_LIMIT) next = JOY_LIMIT;
    if (next < -JOY_LIMIT) next = -JOY_LIMIT;
    if (next == axis->value) return false;
    axis->value = next;
    return true;
}

void joystickCounter_init(void)
{
    x_axis = (joy_axis_t){0};
    y_axis = (joy_axis_t){0};
    initialized = joystick_init();
    adc_error = !initialized;
    last_sample = HAL_GetTick();
}

void joystickCounter_task(void)
{
    if (!initialized) return;
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_sample) < JOY_SAMPLE_MS) return;
    last_sample = now;
    uint16_t vrx, vry;      /* vrx : PA4(IN4), vry : PB0(IN8) */
    adc_error = !joystick_read(&vrx, &vry);
    if (adc_error) {
        x_axis.direction = 0;
        y_axis.direction = 0;
        return;
    }
    (void)axis_update(&x_axis, vry, JOY_X_POLARITY, now);   /* 좌우   -> X좌표 */
    (void)axis_update(&y_axis, vrx, JOY_Y_POLARITY, now);   /* 위아래 -> Y좌표 */
}

int joystickCounter_getX(void) { return x_axis.value; }
int joystickCounter_getY(void) { return y_axis.value; }
bool joystickCounter_hasError(void) { return adc_error; }
