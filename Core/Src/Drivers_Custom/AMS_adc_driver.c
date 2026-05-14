/**
 * @file    AMS_adc_driver.c
 * @brief   ADC driver: DMA acquisition + ISR-safe raw shadow buffer.
 *
 * ISR/Task split
 * --------------
 * HAL_ADC_ConvCpltCallback (ISR context)
 *   → averages the DMA buffer into s_shadow
 *   → notifies the ADC task via xTaskNotifyFromISR
 *   → restarts DMA
 *
 * b_AMS_ADC_PopRawShadow (task context)
 *   → atomically copies s_shadow under a critical section
 *   → returns the snapshot to the caller (no mutex, no RTOS blocking)
 *
 * The Broker is NEVER touched from ISR context.
 *
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Drivers_Custom/AMS_adc_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>
#include <string.h>

/* ====================== PRIVATE VARIABLES ============================== */

static ADC_HandleTypeDef *p_hadc1 = NULL;
static ADC_HandleTypeDef *p_hadc2 = NULL;

/** ADC1: 3-channel scan [CH9-battery | CH18-temperature | CH17-VREFINT] × NUM_MUESTRAS
 *  DMA layout: [bat0, temp0, vref0, bat1, temp1, vref1, ...]
 */
static uint16_t s_dma_buffer_adc1[NUM_MUESTRAS * ADC1_NUM_CHANNELS];
static uint16_t s_dma_buffer_adc2[NUM_MUESTRAS * 2u];

/** Shadow buffer written by the ISR, read atomically by the task. */
static volatile ADC_RawShadow_t s_shadow = {0};
static volatile bool            b_shadow_ready = false;

/** Handle of the ADC task — set once by vd_AMS_ADC_RegisterTask(). */
static TaskHandle_t s_adc_task_handle = NULL;

/* ====================== PUBLIC API ===================================== */

void vd_AMS_ADC_Init(ADC_HandleTypeDef *phadc1,
                     ADC_HandleTypeDef *phadc2,
                     TIM_HandleTypeDef *phtim2,
                     TIM_HandleTypeDef *phtim3)
{
    if (phadc1 == NULL || phadc2 == NULL) {
        return;
    }

    p_hadc1 = phadc1;
    p_hadc2 = phadc2;

    HAL_ADC_Start_DMA(phadc1, (uint32_t *)s_dma_buffer_adc1,
                      NUM_MUESTRAS * ADC1_NUM_CHANNELS);
    HAL_TIM_Base_Start(phtim2);

    HAL_ADC_Start_DMA(phadc2, (uint32_t *)s_dma_buffer_adc2,
                      NUM_MUESTRAS * 2u);
    HAL_TIM_Base_Start(phtim3);
}

void vd_AMS_ADC_RegisterTask(TaskHandle_t task_handle)
{
    s_adc_task_handle = task_handle;
}

bool b_AMS_ADC_PopRawShadow(ADC_RawShadow_t *p_out)
{
    if (p_out == NULL) {
        return false;
    }

    /* Atomic copy: disable interrupts for the minimum time needed to read
     * the five 16-bit fields and the ready flag — no RTOS blocking here. */
    taskENTER_CRITICAL();
    const bool b_has_data = b_shadow_ready;
    if (b_has_data) {
        *p_out = (ADC_RawShadow_t) {
            .adc1_battery_raw  = s_shadow.adc1_battery_raw,
            .adc1_temp_raw     = s_shadow.adc1_temp_raw,
            .adc1_vrefint_raw  = s_shadow.adc1_vrefint_raw,
            .adc2_ch1_raw      = s_shadow.adc2_ch1_raw,
            .adc2_ch2_raw      = s_shadow.adc2_ch2_raw,
        };
        b_shadow_ready = false;
    }
    taskEXIT_CRITICAL();

    return b_has_data;
}

/* ====================== HAL CALLBACK (ISR CONTEXT) ===================== */

/**
 * @brief  DMA conversion-complete callback.
 *         Runs in ISR context — NO Broker calls, NO mutex, NO osDelay.
 *         Writes averaged raw counts to the shadow buffer and wakes the task.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t b_higher_priority_task_woken = pdFALSE;

    /* --- ADC1: 3-channel interleaved scan (battery | temp | VREFINT) --- */
    if (hadc == p_hadc1) {
        uint32_t ui32_sum_battery = 0u;
        uint32_t ui32_sum_temp    = 0u;
        uint32_t ui32_sum_vrefint = 0u;

        for (uint32_t i = 0u; i < NUM_MUESTRAS; i++) {
            ui32_sum_battery += s_dma_buffer_adc1[i * ADC1_NUM_CHANNELS + ADC1_IDX_BATTERY];
            ui32_sum_temp    += s_dma_buffer_adc1[i * ADC1_NUM_CHANNELS + ADC1_IDX_TEMP_SENSOR];
            ui32_sum_vrefint += s_dma_buffer_adc1[i * ADC1_NUM_CHANNELS + ADC1_IDX_VREFINT];
        }

        s_shadow.adc1_battery_raw = (uint16_t)(ui32_sum_battery / NUM_MUESTRAS);
        s_shadow.adc1_temp_raw    = (uint16_t)(ui32_sum_temp    / NUM_MUESTRAS);
        s_shadow.adc1_vrefint_raw = (uint16_t)(ui32_sum_vrefint / NUM_MUESTRAS);

        HAL_ADC_Start_DMA(p_hadc1, (uint32_t *)s_dma_buffer_adc1,
                          NUM_MUESTRAS * ADC1_NUM_CHANNELS);
    }

    /* --- ADC2: 2-channel interleaved scan (suspension 1 | suspension 2) --- */
    else if (hadc == p_hadc2) {
        uint32_t ui32_sum_ch1 = 0u;
        uint32_t ui32_sum_ch2 = 0u;

        for (uint32_t i = 0u; i < (uint32_t)(NUM_MUESTRAS * 2u); i += 2u) {
            ui32_sum_ch1 += s_dma_buffer_adc2[i];
            ui32_sum_ch2 += s_dma_buffer_adc2[i + 1u];
        }

        s_shadow.adc2_ch1_raw = (uint16_t)(ui32_sum_ch1 / NUM_MUESTRAS);
        s_shadow.adc2_ch2_raw = (uint16_t)(ui32_sum_ch2 / NUM_MUESTRAS);

        HAL_ADC_Start_DMA(p_hadc2, (uint32_t *)s_dma_buffer_adc2,
                          NUM_MUESTRAS * 2u);
    }
    else {
        return; /* Unknown ADC — nothing to do. */
    }

    /* Mark shadow as ready and wake the ADC task immediately. */
    b_shadow_ready = true;

    if (s_adc_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_adc_task_handle, &b_higher_priority_task_woken);
        portYIELD_FROM_ISR(b_higher_priority_task_woken);
    }
}
