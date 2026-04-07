/**
 * @file    AMS_DataBroker.c
 * @brief   Implementación del Gestor centralizado de datos del vehículo.
 *          Encapsula el estado global interactuando a través de copias seguras.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Middleware/AMS_DataBroker.h"
#include <string.h>   // Para memcpy
#include <stddef.h>   // Para NULL
#include "cmsis_os.h" // Para FreeRTOS Mutexes

/* ================= VARIABLES ESTATICAS (PRIVADAS) ====================== */
static AMS_ADC_Data_t           s_adc_data          = {0};
static Vehicle_Data_t           s_vehiculo_state    = {0};
static AMS_Persistent_Config_t  s_persistent_config = {0};
static GPS_Data_t               s_gps_data          = {0};

/* Mutexes para acceso concurrente seguro */
static osMutexId_t s_mutex_vehiculo   = NULL;
static osMutexId_t s_mutex_adc        = NULL;
static osMutexId_t s_mutex_persistent = NULL;
static osMutexId_t s_mutex_gps        = NULL;

/* Atributos de los Mutexes */
static const osMutexAttr_t s_mutex_attr = {
  "BrokerMutex",                          /* Human readable name */
  osMutexRecursive | osMutexPrioInherit,  /* attr_bits */
  NULL,                                   /* memory for control block */
  0U                                      /* size of control block */
};
static bool b_is_initialized = false;

/* ================= IMPLEMENTACIÓN DE FUNCIONES ====================== */

void b_Broker_Init(void) {
    /* Inicialización de Mutexes */
    if (s_mutex_vehiculo == NULL) {
        s_mutex_vehiculo = osMutexNew(&s_mutex_attr);
    }
    if (s_mutex_adc == NULL) {
        s_mutex_adc = osMutexNew(&s_mutex_attr);
    }
    if (s_mutex_persistent == NULL) {
        s_mutex_persistent = osMutexNew(&s_mutex_attr);
    }
    if (s_mutex_gps == NULL) {
        s_mutex_gps = osMutexNew(&s_mutex_attr);
    }
    
    // Inicializamos las estructuras a cero por seguridad
    memset(&s_vehiculo_state, 0, sizeof(Vehicle_Data_t));
    memset(&s_adc_data, 0, sizeof(AMS_ADC_Data_t));
    memset(&s_persistent_config, 0, sizeof(AMS_Persistent_Config_t));
    memset(&s_gps_data, 0, sizeof(GPS_Data_t));
    
    b_is_initialized = true;
}

/* --- Getters (Lectura Segura) --- */

bool b_Broker_Get_VehicleState(Vehicle_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) {
        return false;
    }

    if (osMutexAcquire(s_mutex_vehiculo, osWaitForever) != osOK) {
        return false;
    }
    
    // Copia de seguridad del estado privado al puntero del usuario
    memcpy(p_copy, &s_vehiculo_state, sizeof(Vehicle_Data_t));
    
    osMutexRelease(s_mutex_vehiculo);

    return true;
}

bool b_Broker_Get_ADCData(AMS_ADC_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) {
        return false;
    }

    if (osMutexAcquire(s_mutex_adc, osWaitForever) != osOK) {
        return false;
    }
    
    // Copia de seguridad del estado privado al puntero del usuario
    memcpy(p_copy, &s_adc_data, sizeof(AMS_ADC_Data_t));
    
    osMutexRelease(s_mutex_adc);

    return true;
}


/* --- Setters (Escritura Segura) --- */

bool b_Broker_Update_VehicleState(const Vehicle_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) {
        return false;
    }

    if (osMutexAcquire(s_mutex_vehiculo, osWaitForever) != osOK) {
        return false;
    }
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_vehiculo_state, p_new_data, sizeof(Vehicle_Data_t));
    
    osMutexRelease(s_mutex_vehiculo);

    return true;
}

bool b_Broker_Update_ADCData(const AMS_ADC_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) {
        return false;
    }

    if (osMutexAcquire(s_mutex_adc, osWaitForever) != osOK) {
        return false;
    }
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_adc_data, p_new_data, sizeof(AMS_ADC_Data_t));
    
    osMutexRelease(s_mutex_adc);

    return true;
}

/* ================ IMPLEMENTACION: DATOS PERSISTENTES (FLASH) =========== */

void vd_Broker_Set_PersistentConfig(const AMS_Persistent_Config_t *p_config) {
    if (p_config == NULL || !b_is_initialized) return;
    
    if (osMutexAcquire(s_mutex_persistent, osWaitForever) == osOK) {
        memcpy(&s_persistent_config, p_config, sizeof(AMS_Persistent_Config_t));
        osMutexRelease(s_mutex_persistent);
    }
}

bool b_Broker_Get_PersistentConfig(AMS_Persistent_Config_t *p_out) {
    if (p_out == NULL || !b_is_initialized) return false;
    
    if (osMutexAcquire(s_mutex_persistent, osWaitForever) != osOK) {
        return false;
    }
    
    memcpy(p_out, &s_persistent_config, sizeof(AMS_Persistent_Config_t));
    osMutexRelease(s_mutex_persistent);
    
    return true;
}

/* ========================= GPS DATA ============================= */

bool b_Broker_Update_GPSData(const GPS_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) return false;

    if (osMutexAcquire(s_mutex_gps, osWaitForever) != osOK) return false;
    memcpy(&s_gps_data, p_new_data, sizeof(GPS_Data_t));
    osMutexRelease(s_mutex_gps);
    return true;
}

bool b_Broker_Get_GPSData(GPS_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    if (osMutexAcquire(s_mutex_gps, osWaitForever) != osOK) return false;
    memcpy(p_copy, &s_gps_data, sizeof(GPS_Data_t));
    osMutexRelease(s_mutex_gps);
    return true;
}
