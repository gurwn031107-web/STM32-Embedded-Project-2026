/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_1F_Pin GPIO_PIN_0
#define BTN_1F_GPIO_Port GPIOC
#define BTN_2F_Pin GPIO_PIN_1
#define BTN_2F_GPIO_Port GPIOC
#define SENSOR_1F_Pin GPIO_PIN_2
#define SENSOR_1F_GPIO_Port GPIOC
#define SENSOR_1F_EXTI_IRQn EXTI2_IRQn
#define BTN_3F_Pin GPIO_PIN_3
#define BTN_3F_GPIO_Port GPIOC
#define MOTOR_IN1_Pin GPIO_PIN_0
#define MOTOR_IN1_GPIO_Port GPIOA
#define MOTOR_IN2_Pin GPIO_PIN_1
#define MOTOR_IN2_GPIO_Port GPIOA
#define MOTOR_IN3_Pin GPIO_PIN_4
#define MOTOR_IN3_GPIO_Port GPIOA
#define MOTOR_IN4_Pin GPIO_PIN_0
#define MOTOR_IN4_GPIO_Port GPIOB
#define SENSOR_3F_Pin GPIO_PIN_12
#define SENSOR_3F_GPIO_Port GPIOB
#define SENSOR_3F_EXTI_IRQn EXTI15_10_IRQn
#define LED_DOOR_Pin GPIO_PIN_15
#define LED_DOOR_GPIO_Port GPIOB
#define DHT11_Pin GPIO_PIN_8
#define DHT11_GPIO_Port GPIOC
#define SENSOR_2F_Pin GPIO_PIN_10
#define SENSOR_2F_GPIO_Port GPIOC
#define SENSOR_2F_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
