/**
 * @file    AMS_adc_driver.h
 * @brief   ADC driver: DMA acquisition + ISR-safe raw shadow buffer.
 *
 * ISR contract
 * ------------
 * HAL_ADC_ConvCpltCallback writes filtered raw counts into a static volatile
 * shadow buffer and wakes the ADC task via xTaskNotifyFromISR.
 * NO Broker calls are made inside the ISR.
 *
 * Task contract
 * -------------
 * After calling vd_AMS_ADC_RegisterTask() once at startup, the task blocks
 * with xTaskNotifyWait.  When it wakes, it calls b_AMS_ADC_PopRawShadow()
 * to obtain a consistent snapshot and then writes to the Broker.
 *
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef ADC_AMS_H_
#define ADC_AMS_H_

#include "main.h"
#include "AMS_DataTypes.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* ====================== PUBLIC TYPES =================================== */

/**
 * @brief Filtered raw ADC counts produced by the ISR.
 *        Averaged over NUM_MUESTRAS conversions per channel.
 */
typedef struct {
    uint16_t adc1_battery_raw;   /**< ADC1 CH9  — 12 V battery sense  */
    uint16_t adc1_temp_raw;      /**< ADC1 CH18 — internal temp sensor */
    uint16_t adc1_vrefint_raw;   /**< ADC1 CH17 — VREFINT              */
    uint16_t adc2_ch1_raw;       /**< ADC2 CH12 — suspension 1         */
    uint16_t adc2_ch2_raw;       /**< ADC2 CH13 — suspension 2         */
} ADC_RawShadow_t;

/* ====================== PUBLIC API ===================================== */

/**
 * @brief Initialises DMA on ADC1 & ADC2 and starts their trigger timers.
 *        Must be called once from the ADC task before entering the loop.
 *
 * @param phadc1  ADC1 handle (battery | temp | VREFINT scan).
 * @param phadc2  ADC2 handle (suspension channels 1 & 2).
 * @param phtim2  TIM2 handle — ADC1 trigger timer.
 * @param phtim3  TIM3 handle — ADC2 trigger timer.
 */
void vd_AMS_ADC_Init(ADC_HandleTypeDef *phadc1,
                     ADC_HandleTypeDef *phadc2,
                     TIM_HandleTypeDef *phtim2,
                     TIM_HandleTypeDef *phtim3);

/**
 * @brief Registers the ADC task handle so the ISR can wake it.
 *        Must be called from the ADC task before entering the loop,
 *        after vd_AMS_ADC_Init().
 *
 * @param task_handle  Handle of the calling FreeRTOS task.
 */
void vd_AMS_ADC_RegisterTask(TaskHandle_t task_handle);

/**
 * @brief Atomically copies the ISR shadow buffer into @p p_out and
 *        clears the internal ready flag.
 *        Safe to call from task context only (uses a critical section,
 *        NOT a mutex).
 *
 * @param p_out  Destination for the raw shadow snapshot.
 * @return true  New data was available and copied.
 * @return false p_out is NULL or no new data since last pop.
 */
bool b_AMS_ADC_PopRawShadow(ADC_RawShadow_t *p_out);

#endif /* ADC_AMS_H_ */
