#include "ClockNeedle.h"
#include "../driver/StepMotor/StepMotor.h"

/* Each row is ULN2003 IN1, IN2, IN3, IN4. No overlap with FND/LED/I2C. */
static stepMotor_pin_t pins[3] = {
    {{GPIOC, GPIOC, GPIOC, GPIOD}, {GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_2}},
    {{GPIOC, GPIOC, GPIOC, GPIOC}, {GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_6, GPIO_PIN_5}},
    {{GPIOA, GPIOA, GPIOA, GPIOB}, {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_4, GPIO_PIN_0}}
};
static stepMotor_t motors[3];
/* Change an individual entry if that hand rotates counterclockwise. */
static const stepMotor_dir_t forward[3] = {
    STEP_MOTOR_DIR_CW, STEP_MOTOR_DIR_CCW, STEP_MOTOR_DIR_CW
};
static volatile uint16_t target[3];
static uint16_t position[3];
static uint8_t tickDivider;

void ClockNeedle_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            HAL_GPIO_WritePin(pins[i].port[j], pins[i].pin[j], GPIO_PIN_RESET);
            gpio.Pin = pins[i].pin[j];
            HAL_GPIO_Init(pins[i].port[j], &gpio);
        }
        motors[i].pin = &pins[i];
        motors[i].stepCounter = 0;
        position[i] = target[i] = 0;
        /* Energize phase 0: align all physical hands to 12 before starting. */
        StepMotor_hold(&motors[i]);
    }
    tickDivider = 0;
}

void ClockNeedle_setTime(uint8_t hour, uint8_t min, uint8_t sec)
{
    if (hour > 23 || min > 59 || sec > 59) return;
    uint32_t seconds = (uint32_t)(hour % 12U) * 3600U + min * 60U + sec;
    uint16_t next[3] = {
        seconds * NEEDLE_STEPS_PER_REV / 43200U,
        ((uint32_t)min * 60U + sec) * NEEDLE_STEPS_PER_REV / 3600U,
        (uint32_t)sec * NEEDLE_STEPS_PER_REV / 60U
    };
    /* Publish one coherent target set to the interrupt. */
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    for (int i = 0; i < 3; ++i) target[i] = next[i];
    __set_PRIMASK(key);
}

void ClockNeedle_ISR(void)
{
    if (++tickDivider < NEEDLE_STEP_INTERVAL_MS) return;
    tickDivider = 0;
    for (int i = 0; i < 3; ++i) {
        uint16_t distance = (target[i] + NEEDLE_STEPS_PER_REV - position[i])
                            % NEEDLE_STEPS_PER_REV;
        if (distance == 0) continue; /* HOLD the coil pattern; do not release. */
        if (distance <= NEEDLE_STEPS_PER_REV / 2U) {
            StepMotor_stepPhase(&motors[i], forward[i]);
            position[i] = (position[i] + 1U) % NEEDLE_STEPS_PER_REV;
        } else {
            stepMotor_dir_t reverse = forward[i] == STEP_MOTOR_DIR_CW
                                   ? STEP_MOTOR_DIR_CCW : STEP_MOTOR_DIR_CW;
            StepMotor_stepPhase(&motors[i], reverse);
            position[i] = (position[i] + NEEDLE_STEPS_PER_REV - 1U)
                          % NEEDLE_STEPS_PER_REV;
        }
    }
}
