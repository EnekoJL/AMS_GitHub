/**
 * @file    DataBroker.h
 * @brief   Gestor centralizado de datos del vehículo (Pub/Sub pattern).
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef AMS_DATABROKER_H_
#define AMS_DATABROKER_H_

#include "AMS_DataTypes.h"
#include <stdbool.h>

/* --- Inicialización --- */
/**
 * @brief  Inicializa los Mutex / mecanismos de bloqueo del DataBroker.
 *         (Actualmente sin FreeRTOS, prepara las estructuras).
 */
void b_Broker_Init(void);

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

/* ----- Acceso a Datos Persistentes (Flash) ----- */
/**
 * @brief  Carga la configuración persistente en el DataBroker.
 *         Llamado por el Persistence Manager al arrancar.
 * @param  p_config: Puntero a los datos a cargar.
 */
void vd_Broker_Set_PersistentConfig(const AMS_Persistent_Config_t *p_config);

/**
 * @brief  Obtiene una copia de la configuración persistente actual del DataBroker.
 * @param  p_out: Puntero a la estructura donde se copiará la configuración.
 * @retval true si la copia fue exitosa.
 */
bool b_Broker_Get_PersistentConfig(AMS_Persistent_Config_t *p_out);

/* ----- GPS Data ----- */
/**
 * @brief Writes a new GPS snapshot into the DataBroker.
 *        Called by AMS_GPS_Task after each parsed RMC/GGA sentence.
 * @param p_new_data  Pointer to the new GPS_Data_t to store.
 * @retval true if successful.
 */
bool b_Broker_Update_GPSData(const GPS_Data_t *p_new_data);

/**
 * @brief Returns a safe copy of the latest GPS data.
 * @param p_copy  Pointer to the GPS_Data_t that will receive the copy.
 * @retval true if successful.
 */
bool b_Broker_Get_GPSData(GPS_Data_t *p_copy);

/* ----- Telemetry Data ----- */
/**
 * @brief Writes new calculated telemetry into the DataBroker.
 * @param p_new_data  Pointer to the new AMS_Telemetry_Data_t to store.
 * @retval true if successful.
 */
bool b_Broker_Update_TelemetryData(const AMS_Telemetry_Data_t *p_new_data);

/**
 * @brief Returns a safe copy of the latest telemetry data.
 * @param p_copy  Pointer to the AMS_Telemetry_Data_t that will receive the copy.
 * @retval true if successful.
 */
bool b_Broker_Get_TelemetryData(AMS_Telemetry_Data_t *p_copy);

#endif /* AMS_DATABROKER_H_ */
