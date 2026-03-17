/**
 * @file    AMS_adc_driver.c
 * @brief   Implementación del gestor de ADCs.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Drivers_Custom/AMS_adc_driver.h"
#include <stddef.h> 
#include "Middleware/AMS_DataBroker.h" 

/* ================= VARIABLES PRIVADAS (STATIC) ====================== */
static ADC_HandleTypeDef *p_hadc1 = NULL;
static ADC_HandleTypeDef *p_hadc2 = NULL;

/** Buffers persistentes donde escribirá el controlador DMA en background */
static uint16_t s_dma_buffer_adc1[NUM_MUESTRAS];
static uint16_t s_dma_buffer_adc2[NUM_MUESTRAS * 2];

/* ================= IMPLEMENTACIÓN DE FUNCIONES ====================== */

void vd_AMS_ADC_Init(ADC_HandleTypeDef *phadc1, ADC_HandleTypeDef *phadc2, TIM_HandleTypeDef *phtim2, TIM_HandleTypeDef *phtim3) {
    if (phadc1 == NULL || phadc2 == NULL) {
        return; // Proteccion punteros nulos
    }

    p_hadc1 = phadc1;
    p_hadc2 = phadc2;

    // Lanzamos la DMA contra los buffers estaticos locales
    HAL_ADC_Start_DMA(phadc1, (uint32_t*)s_dma_buffer_adc1, NUM_MUESTRAS);
    HAL_TIM_Base_Start(phtim2);

    HAL_ADC_Start_DMA(phadc2, (uint32_t*)s_dma_buffer_adc2, NUM_MUESTRAS * 2);
    HAL_TIM_Base_Start(phtim3);
}

// Funciones de calculo eliminadas, movidas a la Capa de Dominio (Algorithms_Sensors.c)

/* ======================= CALLBACKS DEL HAL ========================== */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    /* 1. Pedimos copia actual al broker */
    AMS_ADC_Data_t local_adc;
    if (!b_Broker_Get_ADCData(&local_adc)) return;

    /* Filtrado para ADC1 */
    if (hadc == p_hadc1) {
        uint32_t suma = 0;
        for (int i = 0; i < NUM_MUESTRAS; i++) {
            suma += s_dma_buffer_adc1[i];
        }
        local_adc.adc1_filtrado = suma / NUM_MUESTRAS;
        
        // Volvemos a lanzar la DMA apuntando al buffer estatico
        HAL_ADC_Start_DMA(p_hadc1, (uint32_t*)s_dma_buffer_adc1, NUM_MUESTRAS);
    }

    /* Filtrado para ADC2 (2 canales) */
    else if (hadc == p_hadc2) {
        uint32_t suma_ch1 = 0;
        uint32_t suma_ch2 = 0;

        for (int i = 0; i < (NUM_MUESTRAS * 2); i += 2) {
            suma_ch1 += s_dma_buffer_adc2[i];
            suma_ch2 += s_dma_buffer_adc2[i + 1];
        }
        local_adc.adc2_ch1_filtrado = suma_ch1 / NUM_MUESTRAS;
        local_adc.adc2_ch2_filtrado = suma_ch2 / NUM_MUESTRAS;
        
        // Volvemos a lanzar la DMA apuntando al buffer estatico
        HAL_ADC_Start_DMA(p_hadc2, (uint32_t*)s_dma_buffer_adc2, NUM_MUESTRAS * 2);
    }
    
    /* 2. Devolvemos los datos procesados al broker */
    b_Broker_Update_ADCData(&local_adc);
}
