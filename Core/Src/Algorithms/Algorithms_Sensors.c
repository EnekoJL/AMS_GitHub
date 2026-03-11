/**
 * @file    Algorithms_Sensors.c
 * @brief   Lógica algorítmica de dominio para los sensores analógicos (ADC).
 *          ESTA CAPA NO DEBE TENER LLAMADAS A HAL NI A FREERTOS.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Algorithms/Algorithms_Sensors.h"
#include <stddef.h>

void Algorithms_Sensors_ProcessVoltages(AMS_ADC_Data_t *p_adc_data) {
    if (p_adc_data == NULL) return;

    // Convertimos el valor crudo del ADC de 12 bits (0-4095) a milivoltios (0-3300mV)
    p_adc_data->voltaje_adc1_mV = (p_adc_data->adc1_filtrado * 3300u) / 4095u;
    p_adc_data->voltaje_adc2_ch1_mV = (p_adc_data->adc2_ch1_filtrado * 3300u) / 4095u;
    p_adc_data->voltaje_adc2_ch2_mV = (p_adc_data->adc2_ch2_filtrado * 3300u) / 4095u;
}

void Algorithms_Sensors_CalculateVehicleData(const AMS_ADC_Data_t *p_adc_data, Vehicle_Data_t *p_veh_data) {
    if (p_adc_data == NULL || p_veh_data == NULL) return;

    /* ------------------------------------------------------------------
     * 1. Batería 12V (Divisor: (10K + 2.7K) / 2.7K = 127 / 27)
     * ------------------------------------------------------------------ */
    p_veh_data->bateria_12v_mV = (p_adc_data->voltaje_adc1_mV * 127u) / 27u;

    /* ------------------------------------------------------------------
     * 2. Suspensión 1 (Divisor: 139 / 100) - Rango: 150mm
     * ------------------------------------------------------------------ */
    // a) Tensión real en el potenciómetro (en milivoltios)
    uint32_t v_real_susp1_mV = (p_adc_data->voltaje_adc2_ch1_mV * 139u) / 100u;

    // b) Regla de 3 para décimas de milímetro (* 10)
    // Formula: (mV_leidos * Rango_mm * 10) / mV_alimentacion
    p_veh_data->recorrido_susp_1_dmm = (v_real_susp1_mV * RANGO_POT_SUSP_1_MM * 10u) / VOLTAJE_ALIMENTACION_POT_MV;

    /* ------------------------------------------------------------------
     * 3. Suspensión 2 (Divisor: 139 / 100) - Rango: 50mm
     * ------------------------------------------------------------------ */
    uint32_t v_real_susp2_mV = (p_adc_data->voltaje_adc2_ch2_mV * 139u) / 100u;

    p_veh_data->recorrido_susp_2_dmm = (v_real_susp2_mV * RANGO_POT_SUSP_2_MM * 10u) / VOLTAJE_ALIMENTACION_POT_MV;
}
