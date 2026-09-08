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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief FreeRTOS stack-overflow hook (configCHECK_FOR_STACK_OVERFLOW in
 *        FreeRTOSConfig.h). Overrides the __WEAK no-op default in cmsis_os2.c.
 *
 *        Deliberately does the bare minimum: the stack that just overflowed
 *        may have trashed adjacent RAM, so this does NOT go through the LED
 *        driver, the Broker, or printf (all of which use more stack and
 *        state than we can trust right now) — just a direct register write
 *        to light the red LED, then halt with interrupts off so a debugger
 *        or watchdog reset can take over instead of the corruption
 *        propagating further.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* RED, active-low: RESET = ON */

    __disable_irq();
    for (;;) {
        /* Halt here — do not attempt to recover or keep scheduling. */
    }
}

/* USER CODE END Application */

