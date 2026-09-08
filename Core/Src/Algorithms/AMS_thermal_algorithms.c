/**
 * @file    AMS_thermal_algorithms.c
 * @brief   See AMS_thermal_algorithms.h.
 */

#include "Algorithms/AMS_thermal_algorithms.h"
#include <stddef.h>

bool b_ThermalCalc_Fold(AMS_ThermalAccumulator_t *p_acc,
                         const int16_t *p_cell_temps_cC,
                         uint8_t        ui8_cell_count,
                         AMS_ThermalStats_t *p_out)
{
    if (p_acc == NULL || p_cell_temps_cC == NULL || p_out == NULL || ui8_cell_count == 0U) {
        return false;
    }

    /* This-sample spatial stats: max, min, and which cell each belongs to. */
    int16_t i16_max = p_cell_temps_cC[0];
    int16_t i16_min = p_cell_temps_cC[0];
    uint8_t ui8_max_id = 0U;
    uint8_t ui8_min_id = 0U;
    int64_t i64_sum = 0;

    for (uint8_t i = 0U; i < ui8_cell_count; i++) {
        int16_t i16_t = p_cell_temps_cC[i];
        i64_sum += i16_t;
        if (i16_t > i16_max) { i16_max = i16_t; ui8_max_id = i; }
        if (i16_t < i16_min) { i16_min = i16_t; ui8_min_id = i; }
    }
    int16_t i16_avg  = (int16_t)(i64_sum / ui8_cell_count);
    int16_t i16_delta = (int16_t)(i16_max - i16_min);

    p_out->i16_max_cell_temp_cC = i16_max;
    p_out->i16_min_cell_temp_cC = i16_min;
    p_out->i16_avg_cell_temp_cC = i16_avg;
    p_out->i16_delta_temp_cC    = i16_delta;
    p_out->ui8_hottest_cell_id  = ui8_max_id;
    p_out->ui8_coldest_cell_id  = ui8_min_id;

    /* Session worst-case tracking. */
    if (!p_acc->b_primed) {
        p_acc->b_primed = true;
        p_acc->i16_session_max_temp_cC   = i16_max;
        p_acc->i16_session_max_delta_cC  = i16_delta;
    } else {
        if (i16_max > p_acc->i16_session_max_temp_cC) {
            p_acc->i16_session_max_temp_cC = i16_max;
        }
        if (i16_delta > p_acc->i16_session_max_delta_cC) {
            p_acc->i16_session_max_delta_cC = i16_delta;
        }
    }

    /* Time-average of the spatial mean — deliberately a running mean of
     * i16_avg, not of every individual cell reading, so a pack with more
     * cells doesn't get weighted more heavily than a fixed-size pack would. */
    p_acc->i64_sum_avg_temp_cC += i16_avg;
    p_acc->ui32_samples++;

    p_out->i16_session_max_temp_cC  = p_acc->i16_session_max_temp_cC;
    p_out->i16_session_max_delta_cC = p_acc->i16_session_max_delta_cC;
    p_out->i16_session_avg_temp_cC  = (int16_t)(p_acc->i64_sum_avg_temp_cC / p_acc->ui32_samples);

    return true;
}
