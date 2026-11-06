#include "ap_main.h"
#include "StopWatch/StopWatch.h"
#include "TimeClock/TimeClock.h"
#include "Display/Display.h"
#include "ClockNeedle.h"
#include "../driver/button/button.h"
#include "main.h"

enum {TIME_CLOCK_MODE, STOP_WATCH_MODE};
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
static button_t btnMode;
static int modeState = TIME_CLOCK_MODE;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) { /* 1 ms; independent of LCD traffic */
        Disp_ISR();
        ClockNeedle_ISR();
    } else if (htim->Instance == TIM4) { /* 10 ms */
        TimeClock_ISR();
        StopWatch_ISR(); /* Counts even in clock mode. */
    }
}
void ap_init(void)
{
    StopWatch_init();
    TimeClock_init();
    Disp_init();
    ClockNeedle_init();
    button_init(&btnMode, GPIOC, GPIO_PIN_0);
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK) Error_Handler();
    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) Error_Handler();
}
int ap_main(void)
{
    ap_init();
    uint32_t lastDisplay = HAL_GetTick() - 50U;
    for (;;) {
        if (button_getState(&btnMode) == ACT_RELEASE) {
            modeState = modeState == TIME_CLOCK_MODE ? STOP_WATCH_MODE : TIME_CLOCK_MODE;
            lastDisplay = HAL_GetTick() - 50U;
        }
        TimeClock_task();
        StopWatch_taskEnabled(modeState == STOP_WATCH_MODE);
        TimeClock_t clock = *TimeClock_getTimeData();
        stopWatch_t stopwatch = *StopWatch_getTimeData();
        if (modeState == TIME_CLOCK_MODE)
            ClockNeedle_setTime(clock.hour, clock.min, clock.sec);
        else
            ClockNeedle_setTime(stopwatch.hour, stopwatch.min, stopwatch.sec);
        if ((uint32_t)(HAL_GetTick() - lastDisplay) >= 50U) {
            lastDisplay = HAL_GetTick();
            Disp_setMode(modeState);
            Disp_setStopWatchState(StopWatch_getState());
            if (modeState == TIME_CLOCK_MODE) {
                Disp_setTimeClockTime(&clock);
                Disp_setTimeClockDate(TimeClock_getDateData());
            } else {
                Disp_setStopWatchTime(&stopwatch);
                Disp_setLcdTimeClockTime(&clock);
            }
        }
    }
}
