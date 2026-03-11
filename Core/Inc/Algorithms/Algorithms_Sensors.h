/**
 * @file    Algorithms_Sensors.h
 * @brief   Lógica algorítmica de dominio para los sensores analógicos (ADC).
 *          Conversión de milivoltios a magnitudes físicas, independiente del Hardware.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#ifndef ALGORITHMS_SENSORS_H_
#define ALGORITHMS_SENSORS_H_

#include "AMS_DataTypes.h"

/**
 * @brief  Procesa los voltajes crudos del ADC (mV) y los convierte en magnitudes
 *         físicas para el vehículo (voltaje de batería y suspensión en dmm).
 * @param  p_adc_data: Puntero a la estructura de entrada (datos crudos, solo lectura).
 * @param  p_veh_data: Puntero a la estructura de salida (datos físicos, se sobreescribe).
 */
void Algorithms_Sensors_CalculateVehicleData(const AMS_ADC_Data_t *p_adc_data, Vehicle_Data_t *p_veh_data);

/**
 * @brief  Realiza la regla de tres interna para sacar los milivoltios de los ADCs en crudo.
 * @param  p_adc_data: Puntero a la estructura IO con los ADCs filtrados
 */
void Algorithms_Sensors_ProcessVoltages(AMS_ADC_Data_t *p_adc_data);

#endif /* ALGORITHMS_SENSORS_H_ */
