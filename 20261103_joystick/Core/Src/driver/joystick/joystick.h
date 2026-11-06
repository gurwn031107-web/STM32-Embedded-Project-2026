#ifndef DRIVER_JOYSTICK_H_
#define DRIVER_JOYSTICK_H_
#include <stdint.h>
#include <stdbool.h>
bool joystick_init(void);
bool joystick_read(uint16_t *x, uint16_t *y);
#endif
