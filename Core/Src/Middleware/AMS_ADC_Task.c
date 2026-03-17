/**
 * @file    AMS_ADC_Task.c
 * @brief   Implementación de la tarea RTOS para procesar el hardware ADC.
 */

#include "Middleware/AMS_ADC_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/AMS_Led_Task.h" /* Opcional, para debug visual */
#include "Algorithms/AMS_sensors.h"
#include "cmsis_os.h" /* Para osDelay */

/**
 * @brief Bucle infinito de la tarea RTOS que procesa el ADC.
 *        Se ejecuta a una frecuencia fija dictaminada por osDelay.
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
