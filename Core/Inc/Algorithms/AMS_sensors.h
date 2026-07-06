/**
 * @file    AMS_sensors.h
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
 *
 *         Pure function: takes the factory ROM calibration values as
 *         parameters instead of reading AMS_VREFINT_CAL_ADDR / TS_CAL1_ADDR /
 *         TS_CAL2_ADDR directly, so it has zero hardware dependency and can
 *         be unit-tested on a host. The caller (AMS_ADC_Task) reads those
 *         ROM addresses once and passes the values in.
 *
 * @param  p_adc_data: Puntero a la estructura IO con los ADCs filtrados
 * @param  ui16_vrefint_cal: Factory VREFINT calibration count (from AMS_VREFINT_CAL_ADDR)
 * @param  ui16_ts_cal1: Factory temp-sensor calibration count at 30°C (from TS_CAL1_ADDR)
 * @param  ui16_ts_cal2: Factory temp-sensor calibration count at 110°C (from TS_CAL2_ADDR)
 */
void Algorithms_Sensors_ProcessVoltages(AMS_ADC_Data_t *p_adc_data,
                                         uint16_t ui16_vrefint_cal,
                                         uint16_t ui16_ts_cal1,
                                         uint16_t ui16_ts_cal2);

#endif /* ALGORITHMS_SENSORS_H_ */
