/**
 * @file    AMS_telemetry_algorithms.h
 * @brief   Domain-layer algorithm for turning successive GPS fixes into
 *          running telemetry (odometer, max/avg speed, max accel/decel).
 *
 *          Extracted out of AMS_Data_Calculator_Task.c's task loop so this
 *          math can be unit-tested directly (a `for(;;)` task loop that
 *          blocks on osDelay() can't be called from a host test). The
 *          accumulator state that used to live in file-scope statics is
 *          now explicit — AMS_TelemetryAccumulator_t — so the function is
 *          pure: same inputs always produce the same outputs, no hidden
 *          global state.
 */
#ifndef ALGORITHMS_TELEMETRY_ALGORITHMS_H_
#define ALGORITHMS_TELEMETRY_ALGORITHMS_H_

#include "AMS_DataTypes.h"

/**
 * @brief Running telemetry accumulator. The task keeps one instance of this
 *        alive for its whole lifetime (a local variable declared before its
 *        `for(;;)` loop) and passes it to b_TelemetryCalc_ProcessFix() on
 *        every iteration.
 */
typedef struct {
    uint32_t ui32_last_processed_tick;
    uint32_t ui32_total_distance_mm;
    int32_t  i32_max_vel_kmh_x1000;
    int64_t  i64_sum_vel_kmh_x1000;
    uint32_t ui32_vel_samples;
    int32_t  i32_max_accel_ms2_x1000;
    int32_t  i32_max_decel_ms2_x1000;
    int32_t  i32_last_vel_ms_x1000;

    /* --- Lifetime baseline, seeded once at boot (see SeedLifetime below) ---
     * Combined with the session fields above on every fold to produce the
     * lifetime_* outputs. Zero until something calls SeedLifetime, which
     * means lifetime == session — a safe, correct default, not a bug. */
    uint32_t ui32_baseline_distance_m;
    int32_t  i32_baseline_max_vel_kmh_x1000;
    int32_t  i32_baseline_max_accel_ms2_x1000;
    int32_t  i32_baseline_max_decel_ms2_x1000;
} AMS_TelemetryAccumulator_t;

/**
 * @brief  Seeds the lifetime baseline fields. Call once, before the first
 *         b_TelemetryCalc_ProcessFix(), from whatever loads historic totals
 *         (currently nothing — see the TODO on AMS_Telemetry_Data_t in
 *         AMS_DataStructs.h). Safe to skip entirely: an un-seeded
 *         accumulator just reports lifetime == session.
 *
 * @param  p_acc                      Accumulator to seed (must be freshly
 *                                     zeroed / not yet folded into).
 * @param  ui32_distance_m            Lifetime distance travelled so far.
 * @param  i32_max_vel_kmh_x1000      Lifetime top speed so far.
 * @param  i32_max_accel_ms2_x1000    Lifetime hardest acceleration so far.
 * @param  i32_max_decel_ms2_x1000    Lifetime hardest deceleration so far
 *                                    (negative — most negative wins).
 */
void vd_TelemetryCalc_SeedLifetime(AMS_TelemetryAccumulator_t *p_acc,
                                    uint32_t ui32_distance_m,
                                    int32_t  i32_max_vel_kmh_x1000,
                                    int32_t  i32_max_accel_ms2_x1000,
                                    int32_t  i32_max_decel_ms2_x1000);

/**
 * @brief  Folds one GPS fix into the running accumulator and derives the
 *         current telemetry snapshot from it (both session and lifetime).
 *
 *         Returns false (accumulator and *p_out_telemetry untouched) when
 *         there is nothing new to process: no GPS fix (`b_gps_is_connected`
 *         false) or the same fix as last time (`ui32_last_fix_tick_ms`
 *         unchanged). Callers should skip writing to the Broker in that case
 *         — matches the original task loop's behaviour exactly.
 *
 * @param  p_acc             Accumulator to read and update in place.
 * @param  p_gps             Latest GPS_Data_t snapshot from the Broker.
 * @param  p_out_telemetry   Filled with the derived telemetry on success.
 * @retval true   a new fix was processed, *p_out_telemetry is valid.
 * @retval false  nothing new (stale/disconnected fix, or NULL argument).
 */
bool b_TelemetryCalc_ProcessFix(AMS_TelemetryAccumulator_t *p_acc,
                                 const GPS_Data_t *p_gps,
                                 AMS_Telemetry_Data_t *p_out_telemetry);

#endif /* ALGORITHMS_TELEMETRY_ALGORITHMS_H_ */
