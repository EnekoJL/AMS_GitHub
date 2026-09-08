/**
 * @file    test_AMS_telemetry_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_telemetry_algorithms.c — pure, no
 *          mocks needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_telemetry_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_telemetry_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Disconnected / stale fix — must reject without touching outputs.
 * ========================================================================= */

void test_ProcessFix_disconnected_gps_returns_false_and_leaves_output_untouched(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected = false;

    AMS_Telemetry_Data_t out = {0};
    out.ui32_total_distance_m = 999; /* sentinel */

    TEST_ASSERT_FALSE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));
    TEST_ASSERT_EQUAL_UINT32(999, out.ui32_total_distance_m);
}

void test_ProcessFix_same_tick_as_last_processed_returns_false(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected     = true;
    gps.ui32_last_fix_tick_ms  = 1000;
    gps.i32_vel_kmh_x1000      = 36000;
    gps.i32_vel_ms_x1000       = 10000;

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out)); /* first fix: accepted */

    /* Same tick again — GPS hasn't produced a new fix yet */
    AMS_Telemetry_Data_t out2 = {0};
    out2.ui32_total_distance_m = 12345; /* sentinel */
    TEST_ASSERT_FALSE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out2));
    TEST_ASSERT_EQUAL_UINT32(12345, out2.ui32_total_distance_m);
}

/* =========================================================================
 * First-ever fix: no previous tick to compute dt against, so distance and
 * accel/decel must stay zero — only max/avg velocity update.
 * ========================================================================= */

void test_ProcessFix_first_fix_updates_velocity_but_not_distance_or_accel(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 36000; /* 36.000 km/h */
    gps.i32_vel_ms_x1000      = 10000; /* 10.000 m/s */

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    TEST_ASSERT_EQUAL_UINT32(0, out.ui32_total_distance_m);
    TEST_ASSERT_EQUAL_INT32(36000, out.i32_max_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(36000, out.i32_avg_vel_kmh_x1000); /* one sample so far */
    TEST_ASSERT_EQUAL_INT32(0, out.i32_max_accel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(0, out.i32_max_decel_ms2_x1000);
}

/* =========================================================================
 * Second fix, 500ms later, faster: distance accumulates, acceleration and
 * max speed/average update. Numbers chosen so every division is exact.
 * ========================================================================= */

void test_ProcessFix_second_fix_accumulates_distance_and_tracks_acceleration(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 36000;
    gps.i32_vel_ms_x1000      = 10000;

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* Second fix: 500ms later, now at 15 m/s (54 km/h) */
    gps.ui32_last_fix_tick_ms = 1500;
    gps.i32_vel_kmh_x1000     = 54000;
    gps.i32_vel_ms_x1000      = 15000;

    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* distance step = (15000 mm/s * 500 ms) / 1000 = 7500 mm = 7.5 m -> truncates to 7 */
    TEST_ASSERT_EQUAL_UINT32(7, out.ui32_total_distance_m);
    /* accel = ((15000-10000) * 1000) / 500 = 10000 (10.000 m/s^2 x1000) */
    TEST_ASSERT_EQUAL_INT32(10000, out.i32_max_accel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(0, out.i32_max_decel_ms2_x1000); /* never decelerated */
    TEST_ASSERT_EQUAL_INT32(54000, out.i32_max_vel_kmh_x1000);
    /* avg = (36000 + 54000) / 2 samples = 45000 */
    TEST_ASSERT_EQUAL_INT32(45000, out.i32_avg_vel_kmh_x1000);
}

/* =========================================================================
 * Deceleration: max_decel_ms2_x1000 must go negative and be tracked
 * independently from max_accel_ms2_x1000.
 * ========================================================================= */

void test_ProcessFix_slowing_down_tracks_max_decel_separately(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 72000;
    gps.i32_vel_ms_x1000      = 20000; /* 20 m/s */

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* Second fix: 200ms later, slowed to 12 m/s */
    gps.ui32_last_fix_tick_ms = 1200;
    gps.i32_vel_kmh_x1000     = 43200;
    gps.i32_vel_ms_x1000      = 12000;

    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* accel = ((12000-20000) * 1000) / 200 = -40000 (-40.000 m/s^2 x1000) */
    TEST_ASSERT_EQUAL_INT32(-40000, out.i32_max_decel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(0, out.i32_max_accel_ms2_x1000); /* never accelerated */
}

/* =========================================================================
 * NULL-pointer safety.
 * ========================================================================= */

void test_ProcessFix_null_pointers_return_false_and_do_not_crash(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    GPS_Data_t gps = {0};
    AMS_Telemetry_Data_t out = {0};

    TEST_ASSERT_FALSE(b_TelemetryCalc_ProcessFix(NULL, &gps, &out));
    TEST_ASSERT_FALSE(b_TelemetryCalc_ProcessFix(&acc, NULL, &out));
    TEST_ASSERT_FALSE(b_TelemetryCalc_ProcessFix(&acc, &gps, NULL));
}

/* =========================================================================
 * Lifetime (baseline + session) — an un-seeded accumulator must report
 * lifetime == session, since baseline defaults to 0 via {0} init.
 * ========================================================================= */

void test_ProcessFix_unseeded_accumulator_lifetime_equals_session(void)
{
    AMS_TelemetryAccumulator_t acc = {0}; /* SeedLifetime never called */
    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 36000;
    gps.i32_vel_ms_x1000      = 10000;

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    TEST_ASSERT_EQUAL_UINT32(out.ui32_total_distance_m,   out.ui32_lifetime_distance_m);
    TEST_ASSERT_EQUAL_INT32(out.i32_max_vel_kmh_x1000,    out.i32_lifetime_max_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(out.i32_max_accel_ms2_x1000,  out.i32_lifetime_max_accel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(out.i32_max_decel_ms2_x1000,  out.i32_lifetime_max_decel_ms2_x1000);
}

/* =========================================================================
 * SeedLifetime: lifetime = baseline + session distance, and MAX(baseline,
 * session) for max speed/accel — session this run is smaller than the
 * seeded baseline, so lifetime max speed/accel must stay at the baseline.
 * ========================================================================= */

void test_ProcessFix_seeded_lifetime_distance_adds_to_baseline(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    vd_TelemetryCalc_SeedLifetime(&acc,
                                   50000,  /* 50 km already on the odometer */
                                   72000,  /* baseline top speed: 72.000 km/h */
                                   20000,  /* baseline max accel: 20.000 m/s^2 */
                                   -20000 /* baseline max decel: -20.000 m/s^2 */);

    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 36000; /* well under the seeded baseline top speed */
    gps.i32_vel_ms_x1000      = 10000;

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* Session distance is 0 on the first fix (no dt yet) -> lifetime == baseline */
    TEST_ASSERT_EQUAL_UINT32(50000, out.ui32_lifetime_distance_m);
    /* This run's speed (36000) never beat the seeded baseline (72000) */
    TEST_ASSERT_EQUAL_INT32(72000, out.i32_lifetime_max_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(20000, out.i32_lifetime_max_accel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(-20000, out.i32_lifetime_max_decel_ms2_x1000);
    /* Session fields are untouched by seeding — still session-only */
    TEST_ASSERT_EQUAL_INT32(36000, out.i32_max_vel_kmh_x1000);
}

/* =========================================================================
 * SeedLifetime: THIS session beats the seeded baseline -> lifetime must
 * pick up the new (higher) session value, not stay stuck at the old one.
 * ========================================================================= */

void test_ProcessFix_session_beats_seeded_baseline_updates_lifetime(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    vd_TelemetryCalc_SeedLifetime(&acc, 0, 36000, 0, 0); /* baseline top speed: 36.000 km/h */

    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 72000; /* new personal best, beats the baseline */
    gps.i32_vel_ms_x1000      = 20000;

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    TEST_ASSERT_EQUAL_INT32(72000, out.i32_lifetime_max_vel_kmh_x1000);
}

/* =========================================================================
 * SeedLifetime: max_decel is stored negative — "worst" (more negative)
 * baseline must win over a milder session decel, and vice versa.
 * ========================================================================= */

void test_ProcessFix_lifetime_max_decel_picks_the_more_negative_value(void)
{
    AMS_TelemetryAccumulator_t acc = {0};
    vd_TelemetryCalc_SeedLifetime(&acc, 0, 0, 0, -50000); /* baseline: -50.000 m/s^2, a hard stop */

    GPS_Data_t gps = {0};
    gps.b_gps_is_connected    = true;
    gps.ui32_last_fix_tick_ms = 1000;
    gps.i32_vel_kmh_x1000     = 72000;
    gps.i32_vel_ms_x1000      = 20000;
    AMS_Telemetry_Data_t out_first = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out_first));

    /* Second fix: mild braking this session (-10.000 m/s^2), much softer
     * than the seeded -50.000 m/s^2 baseline. */
    gps.ui32_last_fix_tick_ms = 1200;
    gps.i32_vel_kmh_x1000     = 68400;
    gps.i32_vel_ms_x1000      = 19000; /* (19000-20000)*1000/200 = -5000 m/s^2 */

    AMS_Telemetry_Data_t out = {0};
    TEST_ASSERT_TRUE(b_TelemetryCalc_ProcessFix(&acc, &gps, &out));

    /* Session decel (-5000) is milder than the baseline (-50000) -> lifetime stays at baseline */
    TEST_ASSERT_EQUAL_INT32(-50000, out.i32_lifetime_max_decel_ms2_x1000);
}
