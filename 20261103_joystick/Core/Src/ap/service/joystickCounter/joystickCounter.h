#ifndef SERVICE_JOYSTICK_COUNTER_H_
#define SERVICE_JOYSTICK_COUNTER_H_
#include <stdbool.h>
void joystickCounter_init(void);
void joystickCounter_task(void);
int joystickCounter_getX(void);
int joystickCounter_getY(void);
bool joystickCounter_hasError(void);
#endif
