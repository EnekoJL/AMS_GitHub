/**
 * @file    DataBroker.h
 * @brief   Gestor centralizado de datos del vehículo (Pub/Sub pattern).
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef DATABROKER_H_
#define DATABROKER_H_

#include "AMS_DataTypes.h"
#include <stdbool.h>

/* --- Inicialización --- */
/**
 * @brief  Inicializa los Mutex / mecanismos de bloqueo del DataBroker.
 *         (Actualmente sin FreeRTOS, prepara las estructuras).
 */
void Broker_Init(void);

/* --- Getters (Lectura Segura) --- */
/**
 * @brief  Obtiene una copia segura de los datos físicos del vehículo.
 * @param  p_copy: Puntero a la estructura donde se copiarán los datos.
 * @retval bool: true si la copia fue exitosa, false en caso de error/bloqueo.
 */
bool b_Broker_Get_VehicleState(Vehicle_Data_t *p_copy);

/**
 * @brief  Obtiene una copia segura de los datos crudos del ADC.
 * @param  p_copy: Puntero a la estructura donde se copiarán los datos.
 * @retval bool: true si la copia fue exitosa, false en caso de error/bloqueo.
 */
bool b_Broker_Get_ADCData(AMS_ADC_Data_t *p_copy);

/* --- Setters (Escritura Segura) --- */
/**
 * @brief  Actualiza el estado físico del vehículo de forma segura.
 * @param  p_new_data: Puntero a la estructura con los nuevos datos.
 * @retval bool: true si la actualización fue exitosa, false en caso de error/bloqueo.
 */
bool b_Broker_Update_VehicleState(const Vehicle_Data_t *p_new_data);

/**
 * @brief  Actualiza los datos crudos del ADC de forma segura.
 * @param  p_new_data: Puntero a la estructura con los nuevos datos.
 * @retval bool: true si la actualización fue exitosa, false en caso de error/bloqueo.
 */
bool b_Broker_Update_ADCData(const AMS_ADC_Data_t *p_new_data);

#endif /* DATABROKER_H_ */
