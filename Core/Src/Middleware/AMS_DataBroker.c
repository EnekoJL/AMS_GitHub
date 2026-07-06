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
static AMS_Telemetry_Data_t     s_telemetry_data    = {0};
static AMS_BMS_Data_t           s_bms_data          = {0};

/* Mutexes para acceso concurrente seguro */
static osMutexId_t s_mutex_vehiculo   = NULL;
static osMutexId_t s_mutex_adc        = NULL;
static osMutexId_t s_mutex_persistent = NULL;
static osMutexId_t s_mutex_gps        = NULL;
static osMutexId_t s_mutex_telemetry  = NULL;
static osMutexId_t s_mutex_bms        = NULL;

/* Last-update timestamps per domain, used only for freshness/safety-flag
 * reporting (b_Broker_Get_SafetyFlags). Not part of the public structs:
 * stamped automatically here on every Update_*, so no producer task has
 * to remember to fill one in. Plain 32-bit ticks — no mutex needed, a
 * torn read here only affects a diagnostic flag, never the data itself. */
static volatile uint32_t s_vehiculo_last_update_ms   = 0;
static volatile uint32_t s_adc_last_update_ms        = 0;
static volatile uint32_t s_gps_last_update_ms         = 0;
static volatile uint32_t s_telemetry_last_update_ms  = 0;
static volatile uint32_t s_bms_last_update_ms         = 0;

/* Explicit "has this domain ever been written" flags. Needed because a
 * tick-based sentinel doesn't work here: HAL ticks wrap as unsigned, so
 * no fixed timestamp value reliably reads as "infinitely long ago" right
 * after boot. A plain bool is simpler and impossible to get subtly wrong. */
static volatile bool s_vehiculo_has_data  = false;
static volatile bool s_adc_has_data       = false;
static volatile bool s_gps_has_data       = false;
static volatile bool s_telemetry_has_data = false;
static volatile bool s_bms_has_data       = false;

/* Max allowed age (ms) before a domain is reported stale. Tune once real
 * producer rates / consumer needs are known — these are starting points
 * based on each task's documented period. */
#define BROKER_MAX_AGE_VEHICLE_MS     300U   /* fed by ADC (event) + CAN (20ms poll) */
#define BROKER_MAX_AGE_ADC_MS         300U   /* event-driven, DMA-triggered          */
#define BROKER_MAX_AGE_GPS_MS        2000U   /* NMEA bursts, ~1 Hz typical            */
#define BROKER_MAX_AGE_TELEMETRY_MS   300U   /* AMS_Data_Calculator_Task, 10ms poll   */
#define BROKER_MAX_AGE_BMS_MS         500U   /* placeholder: no producer task yet     */

/* Atributos de los Mutexes */
static const osMutexAttr_t s_mutex_attr = {
  "BrokerMutex",                          /* Human readable name */
  osMutexRecursive | osMutexPrioInherit,  /* attr_bits */
  NULL,                                   /* memory for control block */
  0U                                      /* size of control block */
};
static bool b_is_initialized = false;

/* Bounded mutex wait: a Get/Set must never block another task forever.
 * If this trips, some task is holding a broker mutex too long (bug) —
 * counted here and surfaced by AMS_Logger_Task via the RED LED. */
#define BROKER_MUTEX_TIMEOUT_MS  50U
static volatile uint32_t s_broker_fault_count = 0;

static inline osStatus_t e_Broker_MutexAcquire(osMutexId_t mutex) {
    osStatus_t status = osMutexAcquire(mutex, pdMS_TO_TICKS(BROKER_MUTEX_TIMEOUT_MS));
    if (status != osOK) {
        s_broker_fault_count++;
    }
    return status;
}

uint32_t u32_Broker_GetFaultCount(void) {
    return s_broker_fault_count;
}

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
    if (s_mutex_telemetry == NULL) {
        s_mutex_telemetry = osMutexNew(&s_mutex_attr);
    }
    if (s_mutex_bms == NULL) {
        s_mutex_bms = osMutexNew(&s_mutex_attr);
    }

    // Inicializamos las estructuras a cero por seguridad
    memset(&s_vehiculo_state, 0, sizeof(Vehicle_Data_t));
    memset(&s_adc_data, 0, sizeof(AMS_ADC_Data_t));
    memset(&s_persistent_config, 0, sizeof(AMS_Persistent_Config_t));
    memset(&s_gps_data, 0, sizeof(GPS_Data_t));
    memset(&s_telemetry_data, 0, sizeof(AMS_Telemetry_Data_t));
    memset(&s_bms_data, 0, sizeof(AMS_BMS_Data_t));

    s_vehiculo_last_update_ms  = 0;
    s_adc_last_update_ms       = 0;
    s_gps_last_update_ms       = 0;
    s_telemetry_last_update_ms = 0;
    s_bms_last_update_ms       = 0;

    s_vehiculo_has_data  = false;
    s_adc_has_data       = false;
    s_gps_has_data       = false;
    s_telemetry_has_data = false;
    s_bms_has_data       = false;

    b_is_initialized = true;
}

/* --- Getters (Lectura Segura) --- */

bool b_Broker_Get_VehicleState(Vehicle_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) {
        return false;
    }

    if (e_Broker_MutexAcquire(s_mutex_vehiculo) != osOK) {
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

    if (e_Broker_MutexAcquire(s_mutex_adc) != osOK) {
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

    if (e_Broker_MutexAcquire(s_mutex_vehiculo) != osOK) {
        return false;
    }
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_vehiculo_state, p_new_data, sizeof(Vehicle_Data_t));
    s_vehiculo_last_update_ms = osKernelGetTickCount();
    s_vehiculo_has_data = true;

    osMutexRelease(s_mutex_vehiculo);

    return true;
}

bool b_Broker_Update_ADCData(const AMS_ADC_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) {
        return false;
    }

    if (e_Broker_MutexAcquire(s_mutex_adc) != osOK) {
        return false;
    }
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_adc_data, p_new_data, sizeof(AMS_ADC_Data_t));
    s_adc_last_update_ms = osKernelGetTickCount();
    s_adc_has_data = true;

    osMutexRelease(s_mutex_adc);

    return true;
}

/* ================ IMPLEMENTACION: DATOS PERSISTENTES (FLASH) =========== */

void vd_Broker_Set_PersistentConfig(const AMS_Persistent_Config_t *p_config) {
    if (p_config == NULL || !b_is_initialized) return;
    
    if (e_Broker_MutexAcquire(s_mutex_persistent) == osOK) {
        memcpy(&s_persistent_config, p_config, sizeof(AMS_Persistent_Config_t));
        osMutexRelease(s_mutex_persistent);
    }
}

bool b_Broker_Get_PersistentConfig(AMS_Persistent_Config_t *p_out) {
    if (p_out == NULL || !b_is_initialized) return false;
    
    if (e_Broker_MutexAcquire(s_mutex_persistent) != osOK) {
        return false;
    }
    
    memcpy(p_out, &s_persistent_config, sizeof(AMS_Persistent_Config_t));
    osMutexRelease(s_mutex_persistent);
    
    return true;
}

/* ========================= GPS DATA ============================= */

bool b_Broker_Update_GPSData(const GPS_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_gps) != osOK) return false;
    memcpy(&s_gps_data, p_new_data, sizeof(GPS_Data_t));
    s_gps_last_update_ms = osKernelGetTickCount();
    s_gps_has_data = true;
    osMutexRelease(s_mutex_gps);
    return true;
}

bool b_Broker_Get_GPSData(GPS_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_gps) != osOK) return false;
    memcpy(p_copy, &s_gps_data, sizeof(GPS_Data_t));
    osMutexRelease(s_mutex_gps);
    return true;
}

/* ========================= TELEMETRY DATA ======================= */

bool b_Broker_Update_TelemetryData(const AMS_Telemetry_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_telemetry) != osOK) return false;
    memcpy(&s_telemetry_data, p_new_data, sizeof(AMS_Telemetry_Data_t));
    s_telemetry_last_update_ms = osKernelGetTickCount();
    s_telemetry_has_data = true;
    osMutexRelease(s_mutex_telemetry);
    return true;
}

bool b_Broker_Get_TelemetryData(AMS_Telemetry_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_telemetry) != osOK) return false;
    memcpy(p_copy, &s_telemetry_data, sizeof(AMS_Telemetry_Data_t));
    osMutexRelease(s_mutex_telemetry);
    return true;
}

/* ========================= BMS DATA (PLACEHOLDER) ================ */

bool b_Broker_Update_BMSData(const AMS_BMS_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_bms) != osOK) return false;
    memcpy(&s_bms_data, p_new_data, sizeof(AMS_BMS_Data_t));
    s_bms_last_update_ms = osKernelGetTickCount();
    s_bms_has_data = true;
    osMutexRelease(s_mutex_bms);
    return true;
}

bool b_Broker_Get_BMSData(AMS_BMS_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    if (e_Broker_MutexAcquire(s_mutex_bms) != osOK) return false;
    memcpy(p_copy, &s_bms_data, sizeof(AMS_BMS_Data_t));
    osMutexRelease(s_mutex_bms);
    return true;
}

/* ========================= SAFETY FLAGS (FRESHNESS) =============== */

bool b_Broker_Get_SafetyFlags(AMS_Safety_Flags_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    uint32_t ui32_now_ms = osKernelGetTickCount();

    /* Flag only: report staleness, take no action. Reads of the plain
     * uint32_t timestamps are not mutex-protected — worst case is a
     * diagnostic flag off by one tick, never a torn read of real data. */
    p_copy->b_vehicle_data_fresh   = s_vehiculo_has_data  && (ui32_now_ms - s_vehiculo_last_update_ms)  <= BROKER_MAX_AGE_VEHICLE_MS;
    p_copy->b_adc_data_fresh       = s_adc_has_data       && (ui32_now_ms - s_adc_last_update_ms)       <= BROKER_MAX_AGE_ADC_MS;
    p_copy->b_gps_data_fresh       = s_gps_has_data       && (ui32_now_ms - s_gps_last_update_ms)       <= BROKER_MAX_AGE_GPS_MS;
    p_copy->b_bms_data_fresh       = s_bms_has_data       && (ui32_now_ms - s_bms_last_update_ms)       <= BROKER_MAX_AGE_BMS_MS;
    p_copy->b_telemetry_data_fresh = s_telemetry_has_data && (ui32_now_ms - s_telemetry_last_update_ms) <= BROKER_MAX_AGE_TELEMETRY_MS;

    return true;
}

/* ========================= FULL SNAPSHOT =========================== */

bool b_Broker_Get_AllData(AMS_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) return false;

    bool b_ok = true;
    b_ok &= b_Broker_Get_VehicleState(&p_copy->vehicle);
    b_ok &= b_Broker_Get_BMSData(&p_copy->bms);
    b_ok &= b_Broker_Get_ADCData(&p_copy->sensors);
    b_ok &= b_Broker_Get_GPSData(&p_copy->gps);
    b_ok &= b_Broker_Get_TelemetryData(&p_copy->telemetry);
    b_ok &= b_Broker_Get_SafetyFlags(&p_copy->safety);

    return b_ok;
}
