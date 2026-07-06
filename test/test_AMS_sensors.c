/**
 * @file    test_AMS_sensors.c
 * @brief   Unit tests for the Algorithms/AMS_sensors.c domain logic.
 *
 *          This is the only layer in the project with zero hardware/RTOS
 *          dependency, so it runs directly on the host (gcc) with no mocks.
 *          Run with: ceedling test:test_AMS_sensors
 */

#include "unity.h"
#include "Algorithms/AMS_sensors.h"
#include "AMS_DataTypes.h"

TEST_SOURCE_FILE("Algorithms/AMS_sensors.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Algorithms_Sensors_ProcessVoltages
 * ========================================================================= */

/* Ideal case: live VREFINT reading equals the factory calibration count.
 * By the formula (VREFINT_CAL_MV * cal / data), if cal == data, VDDA should
 * come back exactly as VREFINT_CAL_MV (3300 mV) — the "board is running at
 * exactly the voltage it was factory-calibrated at" case. */
void test_ProcessVoltages_VDDA_matches_nominal_when_vrefint_equals_calibration(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw = 1500;  /* arbitrary "calibration" count */

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/650,
                                        /*ts_cal2=*/950);

    TEST_ASSERT_EQUAL_UINT32(VREFINT_CAL_MV, adc.ui32_vdda_actual_mV);
}

/* If the live VREFINT count is HALF the factory calibration count, VDDA
 * must come back as roughly DOUBLE the nominal 3300 mV — this is the whole
 * point of the VREFINT-based supply compensation (RM0386 §13.10). */
void test_ProcessVoltages_VDDA_scales_inversely_with_vrefint_reading(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw = 750;  /* half of the 1500 calibration count */

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/650,
                                        /*ts_cal2=*/950);

    TEST_ASSERT_EQUAL_UINT32(VREFINT_CAL_MV * 2u, adc.ui32_vdda_actual_mV);
}

/* Guard: a zero VREFINT reading (channel not yet populated, e.g. first
 * scan after boot) must not divide by zero — falls back to the nominal
 * VREFINT_CAL_MV instead of crashing or returning garbage. */
void test_ProcessVoltages_zero_vrefint_falls_back_to_nominal(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw = 0;

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/650,
                                        /*ts_cal2=*/950);

    TEST_ASSERT_EQUAL_UINT32(VREFINT_CAL_MV, adc.ui32_vdda_actual_mV);
}

/* A raw ADC count at full-scale (4095, 12-bit max) with VDDA at nominal
 * 3300 mV must convert to (very close to) 3300 mV — sanity check on the
 * raw-counts-to-millivolts scaling formula. */
void test_ProcessVoltages_fullscale_adc_count_converts_to_nominal_voltage(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw = 1500;
    adc.adc1_filtrado    = ADC_MAX_COUNT; /* 4095 */

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/650,
                                        /*ts_cal2=*/950);

    TEST_ASSERT_EQUAL_UINT32(VREFINT_CAL_MV, adc.voltaje_adc1_mV);
}

/* Two-point calibration sanity check: a raw temp-sensor count exactly at
 * TS_CAL1 (scaled to the 3.3V reference) must report exactly TS_CAL1_TEMP_C
 * (30.00 degC == 3000 centi-degC). VDDA is set to VREFINT_CAL_MV so the
 * scaling step in STEP 3 is a no-op (ts_scaled == ts_raw). */
void test_ProcessVoltages_temp_at_cal1_point_reports_cal1_temperature(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw     = 1500; /* == vrefint_cal -> VDDA == VREFINT_CAL_MV */
    adc.ui16_temp_sensor_raw = 650;  /* == ts_cal1 */

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/650,
                                        /*ts_cal2=*/950);

    TEST_ASSERT_EQUAL_INT32((int32_t)TS_CAL1_TEMP_C * 100, adc.i32_mcu_temp_cC);
}

/* Guard: invalid ROM calibration (cal2 <= cal1, e.g. blank/corrupt factory
 * data) must not divide by zero or go negative-span — falls back to
 * reporting TS_CAL1_TEMP_C flat rather than garbage. */
void test_ProcessVoltages_invalid_calibration_span_falls_back_to_cal1_temp(void)
{
    AMS_ADC_Data_t adc = {0};
    adc.ui16_vrefint_raw     = 1500;
    adc.ui16_temp_sensor_raw = 800;

    Algorithms_Sensors_ProcessVoltages(&adc,
                                        /*vrefint_cal=*/1500,
                                        /*ts_cal1=*/900,   /* cal1 > cal2: invalid */
                                        /*ts_cal2=*/800);

    TEST_ASSERT_EQUAL_INT32((int32_t)TS_CAL1_TEMP_C * 100, adc.i32_mcu_temp_cC);
}

/* NULL-safety: must not crash on a NULL pointer. */
void test_ProcessVoltages_null_pointer_does_not_crash(void)
{
    Algorithms_Sensors_ProcessVoltages(NULL, 1500, 650, 950);
    TEST_PASS();
}

/* =========================================================================
 * Algorithms_Sensors_CalculateVehicleData
 * ========================================================================= */

/* 12V battery divider: (10k + 2.7k) / 2.7k = 127/27. A sensed 2700 mV at
 * the ADC pin should read back as ~12700 mV (~12.7 V) at the battery. */
void test_CalculateVehicleData_battery_divider_scales_correctly(void)
{
    AMS_ADC_Data_t adc = {0};
    Vehicle_Data_t veh = {0};
    adc.voltaje_adc1_mV = 2700;

    Algorithms_Sensors_CalculateVehicleData(&adc, &veh);

    TEST_ASSERT_EQUAL_UINT32((2700u * 127u) / 27u, veh.bateria_12v_mV);
}

/* Suspension 1 at full deflection: if the pot outputs the full supply
 * voltage (scaled by the 139/100 divider) the travel should read as the
 * full mechanical range (150 mm == 1500 in tenths-of-mm). */
void test_CalculateVehicleData_suspension1_at_full_scale_reads_full_range(void)
{
    AMS_ADC_Data_t adc = {0};
    Vehicle_Data_t veh = {0};
    /* voltaje_adc2_ch1_mV such that v_real_susp1_mV == VOLTAJE_ALIMENTACION_POT_MV */
    adc.voltaje_adc2_ch1_mV = (VOLTAJE_ALIMENTACION_POT_MV * 100u) / 139u;

    Algorithms_Sensors_CalculateVehicleData(&adc, &veh);

    /* Allow ±1 dmm of integer-division rounding error */
    TEST_ASSERT_UINT32_WITHIN(1u, RANGO_POT_SUSP_1_MM * 10u, veh.recorrido_susp_1_dmm);
}

/* Suspension 2 at zero deflection: zero volts in must be zero travel out. */
void test_CalculateVehicleData_suspension2_at_zero_input_reads_zero(void)
{
    AMS_ADC_Data_t adc = {0};
    Vehicle_Data_t veh = {0};
    adc.voltaje_adc2_ch2_mV = 0;

    Algorithms_Sensors_CalculateVehicleData(&adc, &veh);

    TEST_ASSERT_EQUAL_UINT32(0u, veh.recorrido_susp_2_dmm);
}

/* NULL-safety: must not crash on either NULL pointer. */
void test_CalculateVehicleData_null_pointers_do_not_crash(void)
{
    AMS_ADC_Data_t adc = {0};
    Vehicle_Data_t veh = {0};

    Algorithms_Sensors_CalculateVehicleData(NULL, &veh);
    Algorithms_Sensors_CalculateVehicleData(&adc, NULL);
    TEST_PASS();
}
