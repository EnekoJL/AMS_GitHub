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
