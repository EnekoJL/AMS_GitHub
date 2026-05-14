/**
 * @file    AMS_ADC_Task.c
 * @brief   RTOS task for ADC acquisition and physical-unit conversion.
 *
 * Execution model
 * ---------------
 * The task blocks indefinitely on ulTaskNotifyTake(), which is signalled by
 * HAL_ADC_ConvCpltCallback (ISR) via vTaskNotifyGiveFromISR.
 * On each wake-up the task:
 *   1. Pops the raw shadow buffer (ISR-safe atomic copy, no mutex).
 *   2. Reads the current Broker ADC state (mutex-protected, task context).
 *   3. Runs the voltage and physical-unit algorithms.
 *   4. Writes the updated structs back to the Broker (mutex-protected).
 *
 * There is no osDelay — the task is purely event-driven.
 */

#include "Middleware/AMS_ADC_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"
#include "Algorithms/AMS_sensors.h"
#include "Drivers_Custom/AMS_adc_driver.h"
#include "FreeRTOS.h"
#include "task.h"

/* ====================== TASK INIT ====================================== */

void vd_ADC_Task_Init(ADC_HandleTypeDef *phadc1,
                      ADC_HandleTypeDef *phadc2,
                      TIM_HandleTypeDef *phtim2,
                      TIM_HandleTypeDef *phtim3)
{
    /* Start DMA streams and their trigger timers. */
    vd_AMS_ADC_Init(phadc1, phadc2, phtim2, phtim3);

    /* Register this task so the ISR can wake it directly. */
    vd_AMS_ADC_RegisterTask(xTaskGetCurrentTaskHandle());
}

/* ====================== TASK LOOP ====================================== */

void vd_ADC_Manager_TaskProcess(void)
{
    ADC_RawShadow_t shadow;
    AMS_ADC_Data_t  local_adc;
    Vehicle_Data_t  local_veh;

    for (;;) {
        /* Block until the ISR signals that a new DMA batch is ready.
         * ulTaskNotifyTake clears the notification value on exit (pdTRUE). */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Atomically pop the shadow buffer (critical section, not a mutex). */
        if (!b_AMS_ADC_PopRawShadow(&shadow)) {
            continue; /* Spurious wake — nothing to process. */
        }

        /* ---- Safe Broker read (task context → mutex is valid here) ---- */
        if (!b_Broker_Get_ADCData(&local_adc)) {
            continue;
        }

        /* Transfer ISR-averaged raw counts into the Broker struct. */
        local_adc.adc1_filtrado        = shadow.adc1_battery_raw;
        local_adc.ui16_temp_sensor_raw = shadow.adc1_temp_raw;
        local_adc.ui16_vrefint_raw     = shadow.adc1_vrefint_raw;
        local_adc.adc2_ch1_filtrado    = shadow.adc2_ch1_raw;
        local_adc.adc2_ch2_filtrado    = shadow.adc2_ch2_raw;

        /* Convert raw counts → millivolts. */
        Algorithms_Sensors_ProcessVoltages(&local_adc);

        /* Read the current vehicle state to avoid overwriting unrelated fields. */
        if (!b_Broker_Get_VehicleState(&local_veh)) {
            continue;
        }

        /* Convert millivolts → physical units (mm, °C, V …). */
        Algorithms_Sensors_CalculateVehicleData(&local_adc, &local_veh);

        /* ---- Safe Broker write (task context → mutex is valid here) --- */
        b_Broker_Update_ADCData(&local_adc);
        b_Broker_Update_VehicleState(&local_veh);

        /* Heartbeat LED — signals the ADC task is alive and processing. */
        vd_LED_Manager_SetMode(LED_COLOR_ORANGE, LED_PIN_BLINK);
    }
}
