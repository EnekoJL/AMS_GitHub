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
static AMS_Powertrain_Data_t    s_powertrain_data   = {0};
static AMS_BatteryStats_Data_t  s_battery_stats     = {0};

/* Mutexes para acceso concurrente seguro */
static osMutexId_t s_mutex_vehiculo   = NULL;
static osMutexId_t s_mutex_adc        = NULL;
static osMutexId_t s_mutex_persistent = NULL;
static osMutexId_t s_mutex_gps        = NULL;
static osMutexId_t s_mutex_telemetry  = NULL;
static osMutexId_t s_mutex_bms        = NULL;
static osMutexId_t s_mutex_powertrain = NULL;
static osMutexId_t s_mutex_battery_stats = NULL;

/* Last-update timestamps per domain, used only for freshness/safety-flag
 * reporting (b_Broker_Get_SafetyFlags). Not part of the public structs:
 * stamped automatically here on every Update_*, so no producer task has
 * to remember to fill one in. Plain 32-bit ticks — no mutex needed, a
 * torn read here only affects a diagnostic flag, never the data itself. */
static volatile uint32_t s_vehiculo_last_update_ms    = 0;
static volatile uint32_t s_adc_last_update_ms         = 0;
static volatile uint32_t s_gps_last_update_ms         = 0;
static volatile uint32_t s_telemetry_last_update_ms   = 0;
static volatile uint32_t s_bms_last_update_ms         = 0;
static volatile uint32_t s_powertrain_last_update_ms  = 0;
static volatile uint32_t s_battery_stats_last_update_ms = 0;

/* Explicit "has this domain ever been written" flags. Needed because a
 * tick-based sentinel doesn't work here: HAL ticks wrap as unsigned, so
 * no fixed timestamp value reliably reads as "infinitely long ago" right
 * after boot. A plain bool is simpler and impossible to get subtly wrong. */
static volatile bool s_vehiculo_has_data    = false;
static volatile bool s_adc_has_data         = false;
static volatile bool s_gps_has_data         = false;
static volatile bool s_telemetry_has_data   = false;
static volatile bool s_bms_has_data         = false;
static volatile bool s_powertrain_has_data  = false;
static volatile bool s_battery_stats_has_data = false;

/* Max allowed age (ms) before a domain is reported stale. Tune once real
 * producer rates / consumer needs are known — these are starting points
 * based on each task's documented period. */
#define BROKER_MAX_AGE_VEHICLE_MS     300U   /* fed by ADC, event-driven, DMA-triggered */
#define BROKER_MAX_AGE_ADC_MS         300U   /* event-driven, DMA-triggered          */
#define BROKER_MAX_AGE_GPS_MS        2000U   /* NMEA bursts, ~1 Hz typical            */
#define BROKER_MAX_AGE_TELEMETRY_MS   300U   /* AMS_Data_Calculator_Task, 10ms poll   */
#define BROKER_MAX_AGE_BMS_MS         500U   /* placeholder: no producer task yet     */
#define BROKER_MAX_AGE_POWERTRAIN_MS  300U   /* CAN, 20ms poll of RX queue            */
#define BROKER_MAX_AGE_BATTERY_STATS_MS 1500U /* derived from BMS; no producer yet    */

/* Atributos de los Mutexes.
 * osMutexPrioInherit only (no osMutexRecursive): nothing in this file
 * re-acquires a mutex it already holds, and staying non-recursive means a
 * future bug that DOES nest two calls on the same domain fails loudly
 * (deadlock, caught immediately) instead of silently "working" and hiding
 * a violation of the mutex-ordering rule in Architecture_Overview.md. */
static const osMutexAttr_t s_mutex_attr = {
  "BrokerMutex",                          /* Human readable name */
  osMutexPrioInherit,                     /* attr_bits */
  NULL,                                   /* memory for control block */
  0U                                      /* size of control block */
};
static bool s_is_initialized = false;

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

/* Shared Get/Update bodies. Every domain's Getter/Setter is otherwise
 * identical (Acquire -> memcpy -> [stamp] -> Release), so both live here
 * once instead of being copy-pasted per domain — the copy-paste version
 * is what let past-bugs like a wrong sizeof or a forgotten stamp hide in
 * one domain but not another. Adding a new domain still means adding a
 * static, a mutex, and a max-age #define — this only removes the
 * boilerplate *inside* each Get_X/Update_X function body. */
static bool prv_Broker_Read(osMutexId_t mutex, void *p_dst, const void *p_src, size_t size) {
    if (p_dst == NULL || !s_is_initialized) {
        return false;
    }
    if (e_Broker_MutexAcquire(mutex) != osOK) {
        return false;
    }
    memcpy(p_dst, p_src, size);
    osMutexRelease(mutex);
    return true;
}

static bool prv_Broker_Write(osMutexId_t mutex, void *p_dst, const void *p_src, size_t size,
                              volatile uint32_t *p_last_update_ms, volatile bool *p_has_data) {
    if (p_src == NULL || !s_is_initialized) {
        return false;
    }
    if (e_Broker_MutexAcquire(mutex) != osOK) {
        return false;
    }
    memcpy(p_dst, p_src, size);
    if (p_last_update_ms != NULL) {
        *p_last_update_ms = osKernelGetTickCount();
    }
    if (p_has_data != NULL) {
        *p_has_data = true;
    }
    osMutexRelease(mutex);
    return true;
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
    if (s_mutex_powertrain == NULL) {
        s_mutex_powertrain = osMutexNew(&s_mutex_attr);
    }
    if (s_mutex_battery_stats == NULL) {
        s_mutex_battery_stats = osMutexNew(&s_mutex_attr);
    }

    // Inicializamos las estructuras a cero por seguridad
    memset(&s_vehiculo_state, 0, sizeof(Vehicle_Data_t));
    memset(&s_adc_data, 0, sizeof(AMS_ADC_Data_t));
    memset(&s_persistent_config, 0, sizeof(AMS_Persistent_Config_t));
    memset(&s_gps_data, 0, sizeof(GPS_Data_t));
    memset(&s_telemetry_data, 0, sizeof(AMS_Telemetry_Data_t));
    memset(&s_bms_data, 0, sizeof(AMS_BMS_Data_t));
    memset(&s_powertrain_data, 0, sizeof(AMS_Powertrain_Data_t));
    memset(&s_battery_stats, 0, sizeof(AMS_BatteryStats_Data_t));

    s_vehiculo_last_update_ms     = 0;
    s_adc_last_update_ms          = 0;
    s_gps_last_update_ms          = 0;
    s_telemetry_last_update_ms    = 0;
    s_bms_last_update_ms          = 0;
    s_powertrain_last_update_ms   = 0;
    s_battery_stats_last_update_ms = 0;

    s_vehiculo_has_data     = false;
    s_adc_has_data          = false;
    s_gps_has_data          = false;
    s_telemetry_has_data    = false;
    s_bms_has_data          = false;
    s_powertrain_has_data   = false;
    s_battery_stats_has_data = false;

    s_is_initialized = true;
}

/* --- Getters (Lectura Segura) --- */

bool b_Broker_Get_VehicleState(Vehicle_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_vehiculo, p_copy, &s_vehiculo_state, sizeof(Vehicle_Data_t));
}

bool b_Broker_Get_ADCData(AMS_ADC_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_adc, p_copy, &s_adc_data, sizeof(AMS_ADC_Data_t));
}


/* --- Setters (Escritura Segura) --- */

bool b_Broker_Update_VehicleState(const Vehicle_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_vehiculo, &s_vehiculo_state, p_new_data, sizeof(Vehicle_Data_t),
                             &s_vehiculo_last_update_ms, &s_vehiculo_has_data);
}

bool b_Broker_Update_ADCData(const AMS_ADC_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_adc, &s_adc_data, p_new_data, sizeof(AMS_ADC_Data_t),
                             &s_adc_last_update_ms, &s_adc_has_data);
}

/* ================ IMPLEMENTACION: DATOS PERSISTENTES (FLASH) =========== */

void vd_Broker_Set_PersistentConfig(const AMS_Persistent_Config_t *p_config) {
    if (p_config == NULL || !s_is_initialized) return;

    if (e_Broker_MutexAcquire(s_mutex_persistent) == osOK) {
        memcpy(&s_persistent_config, p_config, sizeof(AMS_Persistent_Config_t));
        osMutexRelease(s_mutex_persistent);
    }
}

bool b_Broker_Get_PersistentConfig(AMS_Persistent_Config_t *p_out) {
    return prv_Broker_Read(s_mutex_persistent, p_out, &s_persistent_config, sizeof(AMS_Persistent_Config_t));
}

/* ========================= GPS DATA ============================= */

bool b_Broker_Update_GPSData(const GPS_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_gps, &s_gps_data, p_new_data, sizeof(GPS_Data_t),
                             &s_gps_last_update_ms, &s_gps_has_data);
}

bool b_Broker_Get_GPSData(GPS_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_gps, p_copy, &s_gps_data, sizeof(GPS_Data_t));
}

/* ========================= TELEMETRY DATA ======================= */

bool b_Broker_Update_TelemetryData(const AMS_Telemetry_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_telemetry, &s_telemetry_data, p_new_data, sizeof(AMS_Telemetry_Data_t),
                             &s_telemetry_last_update_ms, &s_telemetry_has_data);
}

bool b_Broker_Get_TelemetryData(AMS_Telemetry_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_telemetry, p_copy, &s_telemetry_data, sizeof(AMS_Telemetry_Data_t));
}

/* ========================= POWERTRAIN DATA (CAN) =================== */

bool b_Broker_Update_PowertrainData(const AMS_Powertrain_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_powertrain, &s_powertrain_data, p_new_data, sizeof(AMS_Powertrain_Data_t),
                             &s_powertrain_last_update_ms, &s_powertrain_has_data);
}

bool b_Broker_Get_PowertrainData(AMS_Powertrain_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_powertrain, p_copy, &s_powertrain_data, sizeof(AMS_Powertrain_Data_t));
}

/* ========================= BATTERY STATS (PLACEHOLDER) ============ */

bool b_Broker_Update_BatteryStats(const AMS_BatteryStats_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_battery_stats, &s_battery_stats, p_new_data, sizeof(AMS_BatteryStats_Data_t),
                             &s_battery_stats_last_update_ms, &s_battery_stats_has_data);
}

bool b_Broker_Get_BatteryStats(AMS_BatteryStats_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_battery_stats, p_copy, &s_battery_stats, sizeof(AMS_BatteryStats_Data_t));
}

/* ========================= BMS DATA (PLACEHOLDER) ================ */

bool b_Broker_Update_BMSData(const AMS_BMS_Data_t *p_new_data) {
    return prv_Broker_Write(s_mutex_bms, &s_bms_data, p_new_data, sizeof(AMS_BMS_Data_t),
                             &s_bms_last_update_ms, &s_bms_has_data);
}

bool b_Broker_Get_BMSData(AMS_BMS_Data_t *p_copy) {
    return prv_Broker_Read(s_mutex_bms, p_copy, &s_bms_data, sizeof(AMS_BMS_Data_t));
}

/* ========================= SAFETY FLAGS (FRESHNESS) =============== */

bool b_Broker_Get_SafetyFlags(AMS_Safety_Flags_t *p_copy) {
    if (p_copy == NULL || !s_is_initialized) return false;

    uint32_t ui32_now_ms = osKernelGetTickCount();

    /* Flag only: report staleness, take no action. Reads of the plain
     * uint32_t timestamps are not mutex-protected — worst case is a
     * diagnostic flag off by one tick, never a torn read of real data. */
    p_copy->b_vehicle_data_fresh    = s_vehiculo_has_data    && (ui32_now_ms - s_vehiculo_last_update_ms)    <= BROKER_MAX_AGE_VEHICLE_MS;
    p_copy->b_adc_data_fresh        = s_adc_has_data         && (ui32_now_ms - s_adc_last_update_ms)         <= BROKER_MAX_AGE_ADC_MS;
    p_copy->b_gps_data_fresh        = s_gps_has_data         && (ui32_now_ms - s_gps_last_update_ms)         <= BROKER_MAX_AGE_GPS_MS;
    p_copy->b_bms_data_fresh        = s_bms_has_data         && (ui32_now_ms - s_bms_last_update_ms)         <= BROKER_MAX_AGE_BMS_MS;
    p_copy->b_telemetry_data_fresh  = s_telemetry_has_data   && (ui32_now_ms - s_telemetry_last_update_ms)   <= BROKER_MAX_AGE_TELEMETRY_MS;
    p_copy->b_powertrain_data_fresh = s_powertrain_has_data  && (ui32_now_ms - s_powertrain_last_update_ms)  <= BROKER_MAX_AGE_POWERTRAIN_MS;
    p_copy->b_battery_stats_fresh   = s_battery_stats_has_data && (ui32_now_ms - s_battery_stats_last_update_ms) <= BROKER_MAX_AGE_BATTERY_STATS_MS;

    return true;
}

/* ========================= FULL SNAPSHOT =========================== */

bool b_Broker_Get_AllData(AMS_Data_t *p_copy) {
    if (p_copy == NULL || !s_is_initialized) return false;

    bool b_ok = true;
    b_ok &= b_Broker_Get_VehicleState(&p_copy->vehicle);
    b_ok &= b_Broker_Get_BMSData(&p_copy->bms);
    b_ok &= b_Broker_Get_ADCData(&p_copy->sensors);
    b_ok &= b_Broker_Get_GPSData(&p_copy->gps);
    b_ok &= b_Broker_Get_TelemetryData(&p_copy->telemetry);
    b_ok &= b_Broker_Get_PowertrainData(&p_copy->powertrain);
    b_ok &= b_Broker_Get_BatteryStats(&p_copy->battery_stats);
    b_ok &= b_Broker_Get_SafetyFlags(&p_copy->safety);

    return b_ok;
}
