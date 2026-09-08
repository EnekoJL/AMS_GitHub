/**
 * @file    AMS_telemetry_algorithms.c
 * @brief   See AMS_telemetry_algorithms.h. Math moved here verbatim from
 *          AMS_Data_Calculator_Task.c's vd_Calculator_Manager_TaskProcess()
 *          — behaviour is unchanged, only the file-scope statics became an
 *          explicit AMS_TelemetryAccumulator_t passed in by the caller.
 */

#include "Algorithms/AMS_telemetry_algorithms.h"
#include <stddef.h>

void vd_TelemetryCalc_SeedLifetime(AMS_TelemetryAccumulator_t *p_acc,
                                    uint32_t ui32_distance_m,
                                    int32_t  i32_max_vel_kmh_x1000,
                                    int32_t  i32_max_accel_ms2_x1000,
                                    int32_t  i32_max_decel_ms2_x1000)
{
    if (p_acc == NULL) {
        return;
    }
    p_acc->ui32_baseline_distance_m         = ui32_distance_m;
    p_acc->i32_baseline_max_vel_kmh_x1000   = i32_max_vel_kmh_x1000;
    p_acc->i32_baseline_max_accel_ms2_x1000 = i32_max_accel_ms2_x1000;
    p_acc->i32_baseline_max_decel_ms2_x1000 = i32_max_decel_ms2_x1000;
}

bool b_TelemetryCalc_ProcessFix(AMS_TelemetryAccumulator_t *p_acc,
                                 const GPS_Data_t *p_gps,
                                 AMS_Telemetry_Data_t *p_out_telemetry)
{
    if (p_acc == NULL || p_gps == NULL || p_out_telemetry == NULL) {
        return false;
    }
    if (!p_gps->b_gps_is_connected) {
        return false;
    }
    if (p_gps->ui32_last_fix_tick_ms == p_acc->ui32_last_processed_tick) {
        return false;
    }

    int32_t i32_vel_kmh = p_gps->i32_vel_kmh_x1000;
    int32_t i32_vel_ms  = p_gps->i32_vel_ms_x1000;

    if (p_acc->ui32_last_processed_tick != 0) {
        uint32_t ui32_dt_ms = p_gps->ui32_last_fix_tick_ms - p_acc->ui32_last_processed_tick;

        if (ui32_dt_ms > 0) {
            /* Odometer (mm = (mm/s * ms) / 1000) */
            uint64_t ui64_dist_step_mm = ((uint64_t)i32_vel_ms * ui32_dt_ms) / 1000ULL;
            p_acc->ui32_total_distance_mm += (uint32_t)ui64_dist_step_mm;

            /* Acceleration (mm/s^2 = (delta_mm/s * 1000) / dt_ms) */
            int32_t i32_delta_v_ms = i32_vel_ms - p_acc->i32_last_vel_ms_x1000;
            int32_t i32_accel_ms2  = (i32_delta_v_ms * 1000) / (int32_t)ui32_dt_ms;

            if (i32_accel_ms2 > p_acc->i32_max_accel_ms2_x1000) {
                p_acc->i32_max_accel_ms2_x1000 = i32_accel_ms2;
            }
            if (i32_accel_ms2 < p_acc->i32_max_decel_ms2_x1000) {
                p_acc->i32_max_decel_ms2_x1000 = i32_accel_ms2;
            }
        }
    }

    /* Max Velocity */
    if (i32_vel_kmh > p_acc->i32_max_vel_kmh_x1000) {
        p_acc->i32_max_vel_kmh_x1000 = i32_vel_kmh;
    }

    /* Average Velocity */
    p_acc->i64_sum_vel_kmh_x1000 += i32_vel_kmh;
    p_acc->ui32_vel_samples++;
    int32_t i32_avg_vel_kmh_x1000 = (int32_t)(p_acc->i64_sum_vel_kmh_x1000 / p_acc->ui32_vel_samples);

    p_out_telemetry->ui32_total_distance_m   = p_acc->ui32_total_distance_mm / 1000;
    p_out_telemetry->i32_max_vel_kmh_x1000   = p_acc->i32_max_vel_kmh_x1000;
    p_out_telemetry->i32_avg_vel_kmh_x1000   = i32_avg_vel_kmh_x1000;
    p_out_telemetry->i32_max_accel_ms2_x1000 = p_acc->i32_max_accel_ms2_x1000;
    p_out_telemetry->i32_max_decel_ms2_x1000 = p_acc->i32_max_decel_ms2_x1000;

    /* Lifetime = baseline (from SeedLifetime, 0 if never called) folded with
     * the session values just computed above. Distance sums; max speed and
     * max accel take the larger of the two; max decel takes the smaller
     * (more negative) of the two — decel is stored negative, so "worst" is
     * MIN, not MAX. Easy to get backwards, hence the comment. */
    p_out_telemetry->ui32_lifetime_distance_m = p_acc->ui32_baseline_distance_m
                                               + p_out_telemetry->ui32_total_distance_m;

    p_out_telemetry->i32_lifetime_max_vel_kmh_x1000 =
        (p_acc->i32_baseline_max_vel_kmh_x1000 > p_out_telemetry->i32_max_vel_kmh_x1000)
            ? p_acc->i32_baseline_max_vel_kmh_x1000
            : p_out_telemetry->i32_max_vel_kmh_x1000;

    p_out_telemetry->i32_lifetime_max_accel_ms2_x1000 =
        (p_acc->i32_baseline_max_accel_ms2_x1000 > p_out_telemetry->i32_max_accel_ms2_x1000)
            ? p_acc->i32_baseline_max_accel_ms2_x1000
            : p_out_telemetry->i32_max_accel_ms2_x1000;

    p_out_telemetry->i32_lifetime_max_decel_ms2_x1000 =
        (p_acc->i32_baseline_max_decel_ms2_x1000 < p_out_telemetry->i32_max_decel_ms2_x1000)
            ? p_acc->i32_baseline_max_decel_ms2_x1000
            : p_out_telemetry->i32_max_decel_ms2_x1000;

    p_acc->ui32_last_processed_tick = p_gps->ui32_last_fix_tick_ms;
    p_acc->i32_last_vel_ms_x1000    = i32_vel_ms;

    return true;
}
