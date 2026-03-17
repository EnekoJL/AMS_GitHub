/**
 * @file    sd_logger_task.h
 * @brief   Middleware RTOS Task para gestionar el guardado de datos en la SD.
 *          Coordina la obtención de datos del Broker y la escritura al Driver SD.
 */

#ifndef MIDDLEWARE_AMS_LOGGER_TASK_H_
#define MIDDLEWARE_AMS_LOGGER_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el logger, interactuando con la SD para buscar el 
 *        siguiente nombre de archivo disponible (ej: log_001.csv) y crear la cabecera.
 *        Debe ser llamada ANTES de lanzar el kernel RTOS.
 */
void vd_Logger_Init(void);

/**
 * @brief Hilo infinito del Logger para FreeRTOS. 
 *        Obtiene copia segura del DataBroker, la formatea (CSV) y 
 *        llama al driver SD cada X milisegundos con osDelay.
 *        Debe invocarse desde 'SD_Card_Start' en main.c.
 */
void vd_Logger_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_LOGGER_TASK_H_ */
