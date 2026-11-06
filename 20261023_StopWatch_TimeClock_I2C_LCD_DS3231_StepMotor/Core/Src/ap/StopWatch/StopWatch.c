#include "StopWatch.h"
#include "../../driver/button/button.h"

static volatile stopWatch_t stopWatch_time;
static volatile stopWatch_state_t stopWatch_state;
static button_t btn_run_stop, btn_clear;

stopWatch_t *StopWatch_getTimeData(void)
{
    static stopWatch_t snapshot;
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    snapshot = stopWatch_time;
    __set_PRIMASK(key);
    return &snapshot;
}
void StopWatch_ISR(void)
{
    if (stopWatch_state != RUN) return;
    if (++stopWatch_time.msec < 100) return; /* Units of 10 ms, 0..99 */
    stopWatch_time.msec = 0;
    if (++stopWatch_time.sec < 60) return;
    stopWatch_time.sec = 0;
    if (++stopWatch_time.min < 60) return;
    stopWatch_time.min = 0;
    if (++stopWatch_time.hour < 24) return;
    stopWatch_time.hour = 0;
}
stopWatch_state_t StopWatch_getState(void) { return stopWatch_state; }
void StopWatch_init(void)
{
    stopWatch_state = STOP;
    stopWatch_time = (stopWatch_t){0};
    button_init(&btn_run_stop, GPIOC, GPIO_PIN_1);
    button_init(&btn_clear, GPIOC, GPIO_PIN_3);
}
void StopWatch_taskEnabled(uint8_t enabled)
{
    /* Poll in both modes so an old edge cannot fire on mode entry. */
    button_state_t run = button_getState(&btn_run_stop);
    button_state_t clear = button_getState(&btn_clear);
    if (!enabled) return;
    if (run == ACT_PUSH) {
        stopWatch_state = stopWatch_state == RUN ? STOP : RUN;
    } else if (clear == ACT_RELEASE && stopWatch_state == STOP) {
        uint32_t key = __get_PRIMASK();
        __disable_irq();
        stopWatch_time = (stopWatch_t){0};
        __set_PRIMASK(key);
    }
}
void StopWatch_task(void) { StopWatch_taskEnabled(1); }
