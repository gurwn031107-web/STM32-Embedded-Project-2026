#ifndef DISPLAY_H
#define DISPLAY_H
#include "../StopWatch/StopWatch.h"
#include "../TimeClock/TimeClock.h"
/* Same data types as the core: no incompatible structure pointer casts. */
typedef stopWatch_t stopWatchTime_t;
typedef TimeClock_t timeClockTime_t;
typedef TimeDate_t timeClockDate_t;
void Disp_init(void);
void Disp_setMode(int mode);
void Disp_setTimeClockTime(timeClockTime_t *time);
void Disp_setStopWatchTime(stopWatchTime_t *time);
void Disp_setLcdTimeClockTime(timeClockTime_t *time);
void Disp_setTimeClockDate(timeClockDate_t *date);
void Disp_setStopWatchState(int state);
void Disp_ISR(void);
#endif
