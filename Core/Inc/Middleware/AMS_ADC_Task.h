/**
 * @file    AMS_ADC_Task.h
 * @brief   RTOS Task API for processing bare-metal ADC conversions.
 *
 * Usage (from main.c task entry):
 *   void ADC_Start(void *argument) {
 *       vd_ADC_Task_Init(&hadc1, &hadc2, &htim2, &htim3);
 *       vd_ADC_Manager_TaskProcess();
 *   }
 */

#ifndef MIDDLEWARE_AMS_ADC_TASK_H_
#define MIDDLEWARE_AMS_ADC_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Initializes the ADC hardware layer for this task.
 *        Starts DMA transfers on ADC1 and ADC2, and their trigger timers.
 *        Must be called once at the start of ADC_Start() before the task loop.
 *
 * @param phadc1  Pointer to ADC1 handle (12V battery channel).
 * @param phadc2  Pointer to ADC2 handle (suspension channels 1 & 2).
 * @param phtim2  Pointer to TIM2 handle (ADC1 trigger timer).
 * @param phtim3  Pointer to TIM3 handle (ADC2 trigger timer).
 */
void vd_ADC_Task_Init(ADC_HandleTypeDef *phadc1,
                      ADC_HandleTypeDef *phadc2,
                      TIM_HandleTypeDef *phtim2,
                      TIM_HandleTypeDef *phtim3);

/**
 * @brief Infinite RTOS loop: reads filtered ADC data from the Broker,
 *        applies physical-unit algorithms, and writes results back.
 *        Must be called from ADC_Start() after vd_ADC_Task_Init().
 */
void vd_ADC_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_ADC_TASK_H_ */
