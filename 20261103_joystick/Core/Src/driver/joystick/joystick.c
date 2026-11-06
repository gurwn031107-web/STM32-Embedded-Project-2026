/* ADC1 owner: PA4=IN4, PB0=IN8. Software-triggered single conversions. */
#include "joystick.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_adc.h"
static ADC_HandleTypeDef joy_adc;

static bool adc_read(uint32_t channel, uint16_t *result)
{
    ADC_ChannelConfTypeDef config = {0};
    config.Channel = channel;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    if (HAL_ADC_ConfigChannel(&joy_adc, &config) != HAL_OK) return false;
    if (HAL_ADC_Start(&joy_adc) != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(&joy_adc, 2U) != HAL_OK) {
        (void)HAL_ADC_Stop(&joy_adc);
        return false;
    }
    *result = (uint16_t)HAL_ADC_GetValue(&joy_adc);
    return HAL_ADC_Stop(&joy_adc) == HAL_OK;
}

bool joystick_init(void)
{
    joy_adc.Instance = ADC1;
    joy_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    joy_adc.Init.Resolution = ADC_RESOLUTION_12B;
    joy_adc.Init.ScanConvMode = DISABLE;
    joy_adc.Init.ContinuousConvMode = DISABLE;
    joy_adc.Init.DiscontinuousConvMode = DISABLE;
    joy_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    joy_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    joy_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    joy_adc.Init.NbrOfConversion = 1;
    joy_adc.Init.DMAContinuousRequests = DISABLE;
    joy_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    return HAL_ADC_Init(&joy_adc) == HAL_OK;
}

bool joystick_read(uint16_t *x, uint16_t *y)
{
    return adc_read(ADC_CHANNEL_4, x) && adc_read(ADC_CHANNEL_8, y);
}
