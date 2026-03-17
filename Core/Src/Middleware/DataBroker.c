/**
 * @file    DataBroker.c
 * @brief   Implementación del Gestor centralizado de datos del vehículo.
 *          Encapsula el estado global interactuando a través de copias seguras.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Middleware/DataBroker.h"
#include <string.h>   // Para memcpy
#include <stddef.h>   // Para NULL

/* ================= VARIABLES ESTATICAS (PRIVADAS) ====================== */
static AMS_ADC_Data_t           s_adc_data          = {0};
static Vehicle_Data_t           s_vehiculo_state    = {0};
static AMS_Persistent_Config_t  s_persistent_config = {0};

/* [FUTURO FreeRTOS]: Aquí se declararán los Mutex estáticos.
 *   static SemaphoreHandle_t s_mutex_vehiculo;
 *   static SemaphoreHandle_t s_mutex_adc;
 *   static SemaphoreHandle_t s_mutex_leds;
 */
static bool b_is_initialized = false;

/* ================= IMPLEMENTACIÓN DE FUNCIONES ====================== */

void Broker_Init(void) {
    // Aquí se inicializarán los Mutex estáticos de FreeRTOS en el futuro.
    // Ejemplo: s_mutex_vehiculo = xSemaphoreCreateMutexStatic(&xMutexBufferVehiculo);
    
    // Inicializamos las estructuras a cero por seguridad
    memset(&s_vehiculo_state, 0, sizeof(Vehicle_Data_t));
    memset(&s_adc_data, 0, sizeof(AMS_ADC_Data_t));
    
    b_is_initialized = true;
}

/* --- Getters (Lectura Segura) --- */

bool b_Broker_Get_VehicleState(Vehicle_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) {
        return false;
    }

    // [FUTURO FreeRTOS]: xSemaphoreTake(s_mutex_vehiculo, portMAX_DELAY);
    
    // Copia de seguridad del estado privado al puntero del usuario
    memcpy(p_copy, &s_vehiculo_state, sizeof(Vehicle_Data_t));
    
    // [FUTURO FreeRTOS]: xSemaphoreGive(s_mutex_vehiculo);

    return true;
}

bool b_Broker_Get_ADCData(AMS_ADC_Data_t *p_copy) {
    if (p_copy == NULL || !b_is_initialized) {
        return false;
    }

    // [FUTURO FreeRTOS]: xSemaphoreTake(s_mutex_adc, portMAX_DELAY);
    
    // Copia de seguridad del estado privado al puntero del usuario
    memcpy(p_copy, &s_adc_data, sizeof(AMS_ADC_Data_t));
    
    // [FUTURO FreeRTOS]: xSemaphoreGive(s_mutex_adc);

    return true;
}


/* --- Setters (Escritura Segura) --- */

bool b_Broker_Update_VehicleState(const Vehicle_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) {
        return false;
    }

    // [FUTURO FreeRTOS]: xSemaphoreTake(s_mutex_vehiculo, portMAX_DELAY);
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_vehiculo_state, p_new_data, sizeof(Vehicle_Data_t));
    
    // [FUTURO FreeRTOS]: xSemaphoreGive(s_mutex_vehiculo);

    return true;
}

bool b_Broker_Update_ADCData(const AMS_ADC_Data_t *p_new_data) {
    if (p_new_data == NULL || !b_is_initialized) {
        return false;
    }

    // [FUTURO FreeRTOS]: xSemaphoreTake(s_mutex_adc, portMAX_DELAY);
    
    // Copia de los nuevos datos al estado estructural central
    memcpy(&s_adc_data, p_new_data, sizeof(AMS_ADC_Data_t));
    
    // [FUTURO FreeRTOS]: xSemaphoreGive(s_mutex_adc);

    return true;
}

/* ================ IMPLEMENTACION: DATOS PERSISTENTES (FLASH) =========== */

void vd_Broker_Set_PersistentConfig(const AMS_Persistent_Config_t *p_config) {
    if (p_config == NULL || !b_is_initialized) return;
    // [FUTURO FreeRTOS]: Mutex Take
    memcpy(&s_persistent_config, p_config, sizeof(AMS_Persistent_Config_t));
    // [FUTURO FreeRTOS]: Mutex Give
}

bool b_Broker_Get_PersistentConfig(AMS_Persistent_Config_t *p_out) {
    if (p_out == NULL || !b_is_initialized) return false;
    // [FUTURO FreeRTOS]: Mutex Take
    memcpy(p_out, &s_persistent_config, sizeof(AMS_Persistent_Config_t));
    // [FUTURO FreeRTOS]: Mutex Give
    return true;
}
