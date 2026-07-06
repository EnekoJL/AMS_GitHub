/**
 * @file    Algorithms_Sensors.c
 * @brief   Domain-layer algorithms for analog sensor processing.
 *
 *          ProcessVoltages():  converts raw ADC counts to millivolts using the
 *          actual board VDD derived from the VREFINT channel, and computes MCU
 *          junction temperature from the internal temperature sensor.
 *
 *          CalculateVehicleData(): converts millivolts to physical units
 *          (battery voltage, suspension travel).
 *
 *          NO HAL or FreeRTOS calls allowed in this file. Factory ROM
 *          calibration values are passed in as parameters by the caller,
 *          not read directly here — keeps this file pure and host-testable.
 * @author  Eneko Juanena
 * @date    6 de Marzo de 2026
 */

#include "Algorithms/AMS_sensors.h"
#include "AMS_DataTypes.h"
#include <stddef.h>
#include <stdint.h>

void Algorithms_Sensors_ProcessVoltages(AMS_ADC_Data_t *p_adc_data,
                                         uint16_t ui16_vrefint_cal,
                                         uint16_t ui16_ts_cal1,
                                         uint16_t ui16_ts_cal2) {
    if (p_adc_data == NULL) return;

    /* =====================================================================
     * STEP 1 — Compute actual board VDD using VREFINT factory calibration.
     *
     * The 32F469IDISCOVERY has no stable external Vref: VREF+ is tied to
     * VDD_MCU through a ferrite bead, so the ADC reference fluctuates with
     * the supply. The internal VREFINT channel lets us correct for this.
     *
     * Formula (RM0386 §13.10):
     *   V_DDA_actual_mV = VREFINT_CAL_MV * VREFINT_CAL / VREFINT_DATA
     *
     *   VREFINT_CAL  : factory 12-bit count stored in ROM at 3.3 V — passed
     *                  in by the caller (read once from AMS_VREFINT_CAL_ADDR).
     *                  Kept out of this function so it stays a pure
     *                  input->output algorithm with no hardware/ROM access,
     *                  and can be unit-tested on a host without a real MCU.
     *   VREFINT_DATA : live averaged count from DMA scan (CH17, Rank 3)
     * ===================================================================== */
    const uint16_t ui16_vrefint_data = p_adc_data->ui16_vrefint_raw;

    uint32_t ui32_vdda_mV;
    if (ui16_vrefint_data == 0u) {
        /* Guard: channel not yet populated — fall back to nominal supply */
        ui32_vdda_mV = VREFINT_CAL_MV;
    } else {
        ui32_vdda_mV = ((uint32_t)VREFINT_CAL_MV * (uint32_t)ui16_vrefint_cal)
                        / (uint32_t)ui16_vrefint_data;
    }

    /* Store actual VDD for diagnostics (visible in Logger / DataBroker) */
    p_adc_data->ui32_vdda_actual_mV = ui32_vdda_mV;

    /* =====================================================================
     * STEP 2 — Convert raw ADC counts to millivolts using the actual VDD.
     *
     * Formula: V_mV = (ADC_raw * V_DDA_actual_mV) / 4095
     * ===================================================================== */
    p_adc_data->voltaje_adc1_mV     =
        (p_adc_data->adc1_filtrado     * ui32_vdda_mV) / ADC_MAX_COUNT;
    p_adc_data->voltaje_adc2_ch1_mV =
        (p_adc_data->adc2_ch1_filtrado * ui32_vdda_mV) / ADC_MAX_COUNT;
    p_adc_data->voltaje_adc2_ch2_mV =
        (p_adc_data->adc2_ch2_filtrado * ui32_vdda_mV) / ADC_MAX_COUNT;

    /* =====================================================================
     * STEP 3 — Compute MCU junction temperature from the internal sensor.
     *
     * The internal sensor reading must first be scaled to the factory
     * reference (3.3 V) so it is comparable to the ROM calibration data:
     *   TS_scaled = TS_raw * VREFINT_CAL_MV / V_DDA_actual_mV
     *
     * Then linear interpolation between the two calibration points:
     *   T_cC = ((TS_scaled - TS_CAL1) * (110 - 30) * 100)
     *           / (TS_CAL2 - TS_CAL1)
     *        + (30 * 100)
     *
     * Result is in centi-degrees Celsius (x100). No floating point.
     *   e.g. i32_mcu_temp_cC = 2547 means 25.47 degC
     *
     *   ui16_ts_cal1 / ui16_ts_cal2 : factory ROM values — passed in by the
     *   caller (read once from TS_CAL1_ADDR / TS_CAL2_ADDR), same reasoning
     *   as ui16_vrefint_cal above.
     * ===================================================================== */
    const uint16_t ui16_ts_raw  = p_adc_data->ui16_temp_sensor_raw;

    /* Scale raw reading to the 3.3 V factory reference */
    uint32_t ui32_ts_scaled;
    if (ui32_vdda_mV == 0u) {
        ui32_ts_scaled = (uint32_t)ui16_ts_raw;  /* fallback: no correction */
    } else {
        ui32_ts_scaled = ((uint32_t)ui16_ts_raw * (uint32_t)VREFINT_CAL_MV)
                          / ui32_vdda_mV;
    }

    /* Two-point linear interpolation */
    const int32_t i32_cal_span_cC = (int32_t)(TS_CAL2_TEMP_C - TS_CAL1_TEMP_C) * 100;
    const int32_t i32_count_span  = (int32_t)ui16_ts_cal2 - (int32_t)ui16_ts_cal1;

    if (i32_count_span <= 0) {
        /* Guard: invalid ROM calibration data */
        p_adc_data->i32_mcu_temp_cC = (int32_t)TS_CAL1_TEMP_C * 100;
    } else {
        p_adc_data->i32_mcu_temp_cC =
            (((int32_t)ui32_ts_scaled - (int32_t)ui16_ts_cal1) * i32_cal_span_cC)
            / i32_count_span
            + ((int32_t)TS_CAL1_TEMP_C * 100);
    }
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
