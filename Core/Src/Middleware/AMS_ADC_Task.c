/**
 * @file    AMS_ADC_Task.c
 * @brief   RTOS task for ADC acquisition and physical-unit conversion.
 *
 * Init sequence (called once from ADC_Start in main.c):
 *   vd_ADC_Task_Init()  →  starts DMA on ADC1 & ADC2, starts trigger timers.
 *
 * Task loop (infinite, called from ADC_Start after init):
 *   vd_ADC_Manager_TaskProcess()  →  reads Broker, converts units, writes back.
 */

#include "Middleware/AMS_ADC_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h"       /* Optional: heartbeat LED feedback */
#include "Algorithms/AMS_sensors.h"
#include "Drivers_Custom/AMS_adc_driver.h" /* vd_AMS_ADC_Init */
#include "cmsis_os.h"

/* -----------------------------------------------------------------------
 * Task init
 * ----------------------------------------------------------------------- */

/**
 * @brief Initializes the ADC hardware for this task.
 *        Delegates to the ADC driver: starts DMA on ADC1 & ADC2 and their
 *        respective trigger timers (TIM2 → ADC1, TIM3 → ADC2).
 */
void vd_ADC_Task_Init(ADC_HandleTypeDef *phadc1,
                      ADC_HandleTypeDef *phadc2,
                      TIM_HandleTypeDef *phtim2,
                      TIM_HandleTypeDef *phtim3) {
    vd_AMS_ADC_Init(phadc1, phadc2, phtim2, phtim3);
}

/* -----------------------------------------------------------------------
 * Task loop
 * ----------------------------------------------------------------------- */

/**
 * @brief Infinite RTOS loop: reads filtered ADC raw counts from the Broker,
 *        converts them to millivolts and then to physical units (mm, V),
 *        and writes the results back to the Broker.
 */
void vd_ADC_Manager_TaskProcess(void) {
    /* Variables locales para extraer del Broker */
    AMS_ADC_Data_t local_adc;
    Vehicle_Data_t local_veh;

    /* Infinite loop for the RTOS thread */
    for(;;) {
        /* Extraer los crudos filtrados (ya llenados por la interrupción DMA de hardware) */
        b_Broker_Get_ADCData(&local_adc);
        
        /* 1) Pasar de cuentas ADC (0-4095) a Milivoltios */
        Algorithms_Sensors_ProcessVoltages(&local_adc);

        /* Extraer la estructura actual de magnitudes físicas para no sobreescribir otros datos */
        b_Broker_Get_VehicleState(&local_veh);

        /* 2) Pasar de Milivoltios a Unidades Físicas (Milímetros, etc.) */
        Algorithms_Sensors_CalculateVehicleData(&local_adc, &local_veh);

        /* Guardar ambos structs actualizados de vuelta en el Broker protegido por Mutex */
        b_Broker_Update_ADCData(&local_adc);
        b_Broker_Update_VehicleState(&local_veh);

        /* Opcional: Toggle LED naranja estilo "Heartbeat" de la tarea ADC */
        vd_LED_Manager_SetMode(LED_COLOR_ORANGE, LED_PIN_BLINK);

        /* Bloquear la tarea x milisegundos cediendo CPU a otras tareas (ej. 10ms = 100Hz) */
        osDelay(10); 
    }
}
