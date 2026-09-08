/**
 * @file    DataBroker.h
 * @brief   Gestor centralizado de datos del vehículo (Shared-State / Blackboard
 *          pattern — see Architecture_Overview.md; this is NOT Pub/Sub, there
 *          is no topic subscription or notify-on-change).
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

/**
 * @brief  Number of times a Broker Get/Set has failed to acquire its mutex
 *         within BROKER_MUTEX_TIMEOUT_MS. Should stay 0 in normal operation.
 *         A non-zero/increasing value means some task is holding a broker
 *         mutex too long (bug) — surfaced by AMS_Logger_Task via the RED LED.
 * @retval Total fault count since boot.
 */
uint32_t u32_Broker_GetFaultCount(void);

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

/* ----- Powertrain Data (CAN inverter RPM) ----- */
/**
 * @brief Writes a new powertrain snapshot into the DataBroker.
 *        Called by AMS_CAN_Task after each parsed inverter status frame.
 * @param p_new_data  Pointer to the new AMS_Powertrain_Data_t to store.
 * @retval true if successful.
 */
bool b_Broker_Update_PowertrainData(const AMS_Powertrain_Data_t *p_new_data);

/**
 * @brief Returns a safe copy of the latest powertrain data.
 * @param p_copy  Pointer to the AMS_Powertrain_Data_t that will receive the copy.
 * @retval true if successful.
 */
bool b_Broker_Get_PowertrainData(AMS_Powertrain_Data_t *p_copy);

/* ----- Battery Stats Data (placeholder — see AMS_BatteryStats_Data_t) ----- */
/**
 * @brief Writes a new battery-stats snapshot into the DataBroker.
 *        Will be called by a future BMS task after each charge/thermal/
 *        current fold — no producer exists yet, see AMS_BatteryStats_Data_t.
 * @param p_new_data  Pointer to the new AMS_BatteryStats_Data_t to store.
 * @retval true if successful.
 */
bool b_Broker_Update_BatteryStats(const AMS_BatteryStats_Data_t *p_new_data);

/**
 * @brief Returns a safe copy of the latest battery-stats data.
 * @param p_copy  Pointer to the AMS_BatteryStats_Data_t that will receive the copy.
 * @retval true if successful.
 */
bool b_Broker_Get_BatteryStats(AMS_BatteryStats_Data_t *p_copy);

/* ----- BMS Data (placeholder — see AMS_BMS_Data_t) ----- */
/**
 * @brief Writes a new BMS snapshot into the DataBroker.
 * @param p_new_data  Pointer to the new AMS_BMS_Data_t to store.
 * @retval true if successful.
 */
bool b_Broker_Update_BMSData(const AMS_BMS_Data_t *p_new_data);

/**
 * @brief Returns a safe copy of the latest BMS data.
 * @param p_copy  Pointer to the AMS_BMS_Data_t that will receive the copy.
 * @retval true if successful.
 */
bool b_Broker_Get_BMSData(AMS_BMS_Data_t *p_copy);

/* ----- Safety Flags (freshness) ----- */
/**
 * @brief Computes and returns the current freshness of every data domain.
 *        A domain is "fresh" if its last Update_* call happened within its
 *        allowed max-age window (see BROKER_MAX_AGE_*_MS in the .c file).
 *        Flag only — the Broker takes no corrective action on staleness,
 *        it just reports it. Callers decide what to do.
 * @param p_copy  Pointer to the AMS_Safety_Flags_t that will receive the result.
 * @retval true if successful.
 */
bool b_Broker_Get_SafetyFlags(AMS_Safety_Flags_t *p_copy);

/* ----- Full Snapshot ----- */
/**
 * @brief Convenience: fetches every domain (vehicle, bms, sensors, gps,
 *        telemetry, safety flags) in one call. Internally calls each
 *        individual Getter in turn — no new lock is introduced, so this
 *        is not an atomic "all-or-nothing" snapshot across domains.
 * @param p_copy  Pointer to the AMS_Data_t that will receive the copy.
 * @retval true if every individual Getter succeeded.
 */
bool b_Broker_Get_AllData(AMS_Data_t *p_copy);

#endif /* AMS_DATABROKER_H_ */
