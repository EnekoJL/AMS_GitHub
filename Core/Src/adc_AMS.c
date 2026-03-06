/**
 * @file    adc_AMS.c
 * @brief   Implementación del gestor de ADCs.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "adc_AMS.h"
#include <stddef.h> // Para NULL

/* ================= VARIABLES PRIVADAS (STATIC) ====================== */
static AMS_ADC_Data_t    *p_ams_data = NULL;
static ADC_HandleTypeDef *p_hadc1 = NULL;
static ADC_HandleTypeDef *p_hadc2 = NULL;

/* ================= IMPLEMENTACIÓN DE FUNCIONES ====================== */

void AMS_ADC_Init(AMS_ADC_Data_t *p_data, ADC_HandleTypeDef *phadc1, ADC_HandleTypeDef *phadc2, TIM_HandleTypeDef *phtim2, TIM_HandleTypeDef *phtim3) {
    if (p_data == NULL || phadc1 == NULL || phadc2 == NULL) {
        return; // Proteccion punteros nulos
    }

    p_ams_data = p_data;
    p_hadc1 = phadc1;
    p_hadc2 = phadc2;

    HAL_ADC_Start_DMA(phadc1, (uint32_t*)p_data->buffer_adc1, NUM_MUESTRAS);
    HAL_TIM_Base_Start(phtim2);

    HAL_ADC_Start_DMA(phadc2, (uint32_t*)p_data->buffer_adc2, NUM_MUESTRAS * 2);
    HAL_TIM_Base_Start(phtim3);
}

void AMS_ADC_ProcessVoltages(AMS_ADC_Data_t *p_data) {
    if (p_data == NULL) return;

    p_data->voltaje_adc1_mV = (p_data->adc1_filtrado * 3300) / 4095;
    p_data->voltaje_adc2_ch1_mV = (p_data->adc2_ch1_filtrado * 3300) / 4095;
    p_data->voltaje_adc2_ch2_mV = (p_data->adc2_ch2_filtrado * 3300) / 4095;
}

/* ======================= CALLBACKS DEL HAL ========================== */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    /* Si no hemos inicializado el puntero, salimos */
    if (p_ams_data == NULL) return;

    /* Filtrado para ADC1 */
    if (hadc == p_hadc1) {
        uint32_t suma = 0;
        for (int i = 0; i < NUM_MUESTRAS; i++) {
            suma += p_ams_data->buffer_adc1[i];
        }
        p_ams_data->adc1_filtrado = suma / NUM_MUESTRAS;
    }

    /* Filtrado para ADC2 (2 canales) */
    else if (hadc == p_hadc2) {
        uint32_t suma_ch1 = 0;
        uint32_t suma_ch2 = 0;

        for (int i = 0; i < (NUM_MUESTRAS * 2); i += 2) {
            suma_ch1 += p_ams_data->buffer_adc2[i];
            suma_ch2 += p_ams_data->buffer_adc2[i + 1];
        }
        p_ams_data->adc2_ch1_filtrado = suma_ch1 / NUM_MUESTRAS;
        p_ams_data->adc2_ch2_filtrado = suma_ch2 / NUM_MUESTRAS;
    }
}

void AMS_CalculateVehicleData(AMS_ADC_Data_t *p_adc_data, Vehicle_Data_t *p_veh_data) {
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
