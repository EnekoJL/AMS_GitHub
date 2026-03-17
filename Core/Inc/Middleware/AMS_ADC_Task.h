/**
 * @file    adc_manager_task.h
 * @brief   RTOS Task API for processing bare-metal ADC conversions
 */

#ifndef MIDDLEWARE_AMS_ADC_TASK_H_
#define MIDDLEWARE_AMS_ADC_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Función principal (hilo) del ADC.
 *        Contiene el bucle infinito RTOS (con osDelay) que extrae datos del
 *        Broker, aplica algoritmos físicos y los vuelve a guardar.
 *        Debe ser llamada desde la tarea 'ADC_Start' generada por CubeMX en main.c.
 */
void vd_ADC_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_ADC_TASK_H_ */
