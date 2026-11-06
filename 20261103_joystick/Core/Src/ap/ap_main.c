#include "ap_main.h"
#include "service/joystickCounter/joystickCounter.h"
#include "service/display/display.h"

int ap_main(void)
{
    joystickCounter_init();
    display_init();
    while (1) {
        joystickCounter_task();
        display_task();
    }
}
