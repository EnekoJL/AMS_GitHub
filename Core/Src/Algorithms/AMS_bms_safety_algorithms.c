/**
 * @file    AMS_bms_safety_algorithms.c
 * @brief   See AMS_bms_safety_algorithms.h.
 */
#include "Algorithms/AMS_bms_safety_algorithms.h"
#include <stddef.h>

bool b_BmsSafety_CheckCellVoltage(AMS_CellVoltageFaultState_t *p_state,
                                   const uint16_t *p_cell_mV,
                                   uint16_t        ui16_cell_count,
                                   uint16_t        ui16_uv_threshold_mV,
                                   uint16_t        ui16_ov_threshold_mV)
{
    if ((p_state == NULL) || (p_cell_mV == NULL) || (ui16_cell_count == 0u)) {
        return false;
    }

    bool b_any_out_of_range = false;
    for (uint16_t i = 0; i < ui16_cell_count; i++) {
        if ((p_cell_mV[i] <= ui16_uv_threshold_mV) || (p_cell_mV[i] >= ui16_ov_threshold_mV)) {
            b_any_out_of_range = true;
            break;
        }
    }

    if (b_any_out_of_range) {
        if (p_state->ui8_out_of_range_count < BMS_FAULT_DEBOUNCE_SAMPLES) {
            p_state->ui8_out_of_range_count++;
        }
    } else {
        p_state->ui8_out_of_range_count = 0u;
    }

    return (p_state->ui8_out_of_range_count >= BMS_FAULT_DEBOUNCE_SAMPLES);
}

/* Reference/pull-up values and lookup table ported verbatim from
 * Test_4_09_2025's spi_stm32f4.c (calculate_temperature_lookup()). */
#define NTC_VOLTAGE_REFERENCE_V   3.0f
#define NTC_PULLUP_RESISTOR_OHM   10000.0f
#define NTC_ERROR_SENTINEL_C      (-273.15f)

typedef struct {
    float temperature_C;
    float resistance_ohm;
} AMS_NtcLookupEntry_t;

static const AMS_NtcLookupEntry_t s_ntc_table[] = {
    {-40, 337503.0f}, {-38, 295681.8f}, {-36, 259539.8f}, {-34, 228248.2f},
    {-32, 201100.5f}, {-30, 177496.0f}, {-28, 156922.5f}, {-26, 138968.8f},
    {-24, 123262.2f}, {-22, 109499.3f}, {-20, 97428.0f},  {-18, 86814.2f},
    {-16, 77474.9f},  {-14, 69237.0f},  {-12, 61960.4f},  {-10, 55528.9f},
    {-8, 49828.3f},   {-6, 44775.7f},   {-4, 40288.7f},   {-2, 36298.6f},
    {0, 32747.0f},    {2, 29577.3f},    {4, 26748.9f},    {6, 24221.1f},
    {8, 21958.9f},    {10, 19932.3f},   {12, 18113.7f},   {14, 16480.6f},
    {16, 15011.9f},   {18, 13689.5f},   {20, 12497.7f},   {22, 11422.1f},
    {24, 10450.4f},   {26, 9571.5f},    {28, 8775.8f},    {30, 8054.4f},
    {32, 7399.9f},    {34, 6805.2f},    {36, 6264.5f},    {38, 5772.3f},
    {40, 5323.7f},    {42, 4914.5f},    {44, 4540.9f},    {46, 4199.5f},
    {48, 3887.2f},    {50, 3601.2f},    {52, 3338.9f},    {54, 3098.2f},
    {56, 2877.3f},    {58, 2674.3f},    {60, 2487.5f},    {62, 2315.6f},
    {64, 2157.2f},    {66, 2011.1f},    {68, 1876.3f},    {70, 1751.8f},
    {72, 1636.8f},    {74, 1530.4f},    {76, 1431.9f},    {78, 1340.6f},
    {80, 1256.0f},    {82, 1177.4f},    {84, 1104.4f},    {86, 1036.9f},
    {88, 974.2f},     {90, 916.0f},     {92, 861.6f},     {94, 811.0f},
    {96, 764.0f},     {98, 720.2f},     {100, 679.3f}
};
#define NTC_TABLE_SIZE (sizeof(s_ntc_table) / sizeof(s_ntc_table[0]))

float f_BmsSafety_NtcVoltageToTempC(float f_voltage_reading)
{
    float v_ntc = f_voltage_reading;

    if ((v_ntc <= 0.001f) || (v_ntc >= (NTC_VOLTAGE_REFERENCE_V - 0.001f))) {
        return NTC_ERROR_SENTINEL_C;
    }

    float r_ntc = (NTC_PULLUP_RESISTOR_OHM * v_ntc) / (NTC_VOLTAGE_REFERENCE_V - v_ntc);
    if (r_ntc <= 0.0f) {
        return NTC_ERROR_SENTINEL_C;
    }

    if (r_ntc >= s_ntc_table[0].resistance_ohm) {
        return s_ntc_table[0].temperature_C;
    }
    if (r_ntc <= s_ntc_table[NTC_TABLE_SIZE - 1].resistance_ohm) {
        return s_ntc_table[NTC_TABLE_SIZE - 1].temperature_C;
    }

    for (uint32_t i = 0; i < (NTC_TABLE_SIZE - 1); i++) {
        if ((r_ntc >= s_ntc_table[i + 1].resistance_ohm) && (r_ntc <= s_ntc_table[i].resistance_ohm)) {
            float r1 = s_ntc_table[i].resistance_ohm;
            float r2 = s_ntc_table[i + 1].resistance_ohm;
            float t1 = s_ntc_table[i].temperature_C;
            float t2 = s_ntc_table[i + 1].temperature_C;
            return t1 + ((r_ntc - r1) * (t2 - t1)) / (r2 - r1);
        }
    }

    return NTC_ERROR_SENTINEL_C;
}
