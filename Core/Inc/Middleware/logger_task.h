/**
 * @file    logger_task.h
 * @brief   Middleware (Pseudo-Task) para gestionar el guardado de datos.
 *          Coordina la obtención de datos del Broker y la escritura al Driver SD.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef LOGGER_TASK_H_
#define LOGGER_TASK_H_

/**
 * @brief Inicializa el logger, interactuando con la SD para buscar el 
 *        siguiente nombre de archivo disponible (ej: log_001.csv) y crear la cabecera.
 */
void vd_Logger_Init(void);

/**
 * @brief Obtiene una copia segura de los datos del DataBroker, la formatea 
 *        adecuadamente (CSV) y llama al driver SD para guardarla.
 */
void vd_Logger_Process(void);

#endif /* LOGGER_TASK_H_ */
