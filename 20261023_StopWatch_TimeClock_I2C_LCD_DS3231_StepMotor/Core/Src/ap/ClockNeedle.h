#ifndef CLOCK_NEEDLE_H
#define CLOCK_NEEDLE_H
#include <stdint.h>

/* Nominal 28BYJ-48 half steps/revolution. Calibrate for the actual gearbox. */
#define NEEDLE_STEPS_PER_REV 4096U
#define NEEDLE_STEP_INTERVAL_MS 2U

void ClockNeedle_init(void);
void ClockNeedle_setTime(uint8_t hour, uint8_t min, uint8_t sec);
void ClockNeedle_ISR(void); /* called every 1 ms; GPIO only, no delay/I2C */
#endif
