/**
 * @file    adc_AMS.h
 * @brief   HEADER - Implementación del gestor de ADCs.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef ADC_AMS_H_
#define ADC_AMS_H_

#include "main.h" // Necesario para ADC_HandleTypeDef y TIM_HandleTypeDef
#include "AMS_DataTypes.h"

/* ==================== PROTOTIPOS DE FUNCIONES ======================= */

/**
 * @brief Inicializa las lecturas por DMA y guarda las referencias.
 * @param p_data  Puntero a la instancia de datos que creaste.
 * @param phadc1  Puntero al handle del ADC1.
 * @param phadc2  Puntero al handle del ADC2.
 * @param phtim2  Puntero al Timer del ADC1.
 * @param phtim3  Puntero al Timer del ADC2.
 */
void AMS_ADC_Init(AMS_ADC_Data_t *p_data, ADC_HandleTypeDef *phadc1, ADC_HandleTypeDef *phadc2, TIM_HandleTypeDef *phtim2, TIM_HandleTypeDef *phtim3);

/**
 * @brief Calcula los voltajes leyendo los valores crudos de la estructura.
 * @param p_data  Puntero a la instancia de datos.
 */
void AMS_ADC_ProcessVoltages(AMS_ADC_Data_t *p_data);
void AMS_CalculateVehicleData(AMS_ADC_Data_t *p_adc_data, Vehicle_Data_t *p_veh_data);

#endif /* ADC_AMS_H_ */
