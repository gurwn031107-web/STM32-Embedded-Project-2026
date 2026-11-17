/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ap/task/button_task/button_task.h"
#include "ap/task/motor_control_task/motor_control_task.h"
#include "ap/task/comm_task/comm_task.h"
#include "ap/task/sensor_task/sensor_task.h"   // [자동 구조 추가]
#include "i2c.h"
#include "driver/lcd/lcd.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId MotorTaskHandle;
osThreadId ButtonTaskHandle;
osThreadId CommTaskHandle;
osThreadId SensorTaskHandle;
osMessageQId btnQueHandle;
osMessageQId commQueueHandle;
osMutexId lcdMutexHandle;
osMutexId sensorMutexHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartMotorTask(void const * argument);
void StartButtonTask(void const * argument);
void StartCommTask(void const * argument);
void StartSensorTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = &xIdleStack[0];
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
	/* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of lcdMutex */
  osMutexDef(lcdMutex);
  lcdMutexHandle = osMutexCreate(osMutex(lcdMutex));

  /* definition and creation of sensorMutex */
  osMutexDef(sensorMutex);
  sensorMutexHandle = osMutexCreate(osMutex(sensorMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of btnQue */
  osMessageQDef(btnQue, 16, uint32_t);
  btnQueHandle = osMessageCreate(osMessageQ(btnQue), NULL);

  /* definition and creation of commQueue */
  osMessageQDef(commQueue, 16, uint32_t);
  commQueueHandle = osMessageCreate(osMessageQ(commQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of MotorTask */
  osThreadDef(MotorTask, StartMotorTask, osPriorityNormal, 0, 128);
  MotorTaskHandle = osThreadCreate(osThread(MotorTask), NULL);

  /* definition and creation of ButtonTask */
  osThreadDef(ButtonTask, StartButtonTask, osPriorityNormal, 0, 128);
  ButtonTaskHandle = osThreadCreate(osThread(ButtonTask), NULL);

  /* definition and creation of CommTask */
  osThreadDef(CommTask, StartCommTask, osPriorityNormal, 0, 128);
  CommTaskHandle = osThreadCreate(osThread(CommTask), NULL);

  /* definition and creation of SensorTask */
  osThreadDef(SensorTask, StartSensorTask, osPriorityAboveNormal, 0, 256);
  SensorTaskHandle = osThreadCreate(osThread(SensorTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	if (osMutexWait(lcdMutexHandle, osWaitForever) == osOK) {
		Lcd_init(&hi2c1);
		osMutexRelease(lcdMutexHandle);
	}
	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartMotorTask */
/**
 * @brief Function implementing the MotorTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartMotorTask */
void StartMotorTask(void const * argument)
{
  /* USER CODE BEGIN StartMotorTask */
	motor_control_task_init();
	/* Infinite loop */
	for(;;)
	{
		motor_control_task();
		osDelay(10);
	}
  /* USER CODE END StartMotorTask */
}

/* USER CODE BEGIN Header_StartButtonTask */
/**
 * @brief Function implementing the ButtonTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartButtonTask */
void StartButtonTask(void const * argument)
{
  /* USER CODE BEGIN StartButtonTask */
	button_task_init();
	/* Infinite loop */
	for(;;)
	{
		button_task();
		osDelay(10);
	}
  /* USER CODE END StartButtonTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
 * @brief Function implementing the CommTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartCommTask */
void StartCommTask(void const * argument)
{
  /* USER CODE BEGIN StartCommTask */
	comm_task_init();
	/* Infinite loop */
	for(;;)
	{
		comm_task();
		osDelay(1);
	}
  /* USER CODE END StartCommTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
 * @brief Function implementing the SensorTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void const * argument)
{
  /* USER CODE BEGIN StartSensorTask */
	/* [자동 구조 추가] 초음파 센서 태스크 */
	sensor_task_init();
	/* Infinite loop */
	for(;;)
	{
		sensor_task();   // 센서 3개 측정 (왼쪽 -> 중앙 -> 오른쪽, 사이 15ms)
		osDelay(20);     // 오른쪽 측정 후 다음 왼쪽 측정까지 20ms 간격 (초음파 간섭 방지)
	}
  /* USER CODE END StartSensorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
