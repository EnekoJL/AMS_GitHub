/**
 * @file    AMS_DataTypes.h
 * @brief   Variables globales proyecto
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef AMS_DATATYPES_H_
#define AMS_DATATYPES_H_

#include <stdint.h>

#define NUM_MUESTRAS 10

/* --- CONSTANTES EN ENTEROS --- */
#define VOLTAJE_ALIMENTACION_POT_MV 5000u // 5V en milivoltios
#define RANGO_POT_SUSP_1_MM         150u
#define RANGO_POT_SUSP_2_MM         50u

/**
 * @brief Estructura de hardware (Datos crudos ADC)
 */
typedef struct {
    uint16_t buffer_adc1[NUM_MUESTRAS];
    volatile uint16_t adc1_filtrado;
    uint32_t voltaje_adc1_mV;

    uint16_t buffer_adc2[NUM_MUESTRAS * 2];
    volatile uint16_t adc2_ch1_filtrado;
    volatile uint16_t adc2_ch2_filtrado;
    uint32_t voltaje_adc2_ch1_mV;
    uint32_t voltaje_adc2_ch2_mV;
} AMS_ADC_Data_t;

/**
 * @brief Estructura de aplicación (Magnitudes físicas en enteros)
 */
typedef struct {
    uint32_t bateria_12v_mV;       // Tensión en milivoltios (ej. 12500 -> 12.5V)
    uint32_t recorrido_susp_1_dmm; // Recorrido en décimas de mm (ej. 1452 -> 145.2 mm)
    uint32_t recorrido_susp_2_dmm; // Recorrido en décimas de mm (ej. 485 -> 48.5 mm)
} Vehicle_Data_t;

#endif /* AMS_DATATYPES_H_ */
