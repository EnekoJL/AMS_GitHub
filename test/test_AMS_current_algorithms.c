/**
 * @file    test_AMS_current_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_current_algorithms.c — pure, no
 *          mocks needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_current_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_current_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Discharge and charge peaks tracked independently.
 * ========================================================================= */

void test_Fold_tracks_discharge_and_charge_peaks_separately(void)
{
    AMS_CurrentAccumulator_t acc = {0};
    AMS_CurrentStats_t out = {0};

    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc, -50000, &out)); /* -50 A discharge */
    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc,  20000, &out)); /* +20 A charge */

    TEST_ASSERT_EQUAL_UINT32(50000, out.ui32_session_max_discharge_mA);
    TEST_ASSERT_EQUAL_UINT32(20000, out.ui32_session_max_charge_mA);
}

/* =========================================================================
 * A milder sample after the peak must not overwrite the running max.
 * ========================================================================= */

void test_Fold_milder_sample_does_not_overwrite_running_max(void)
{
    AMS_CurrentAccumulator_t acc = {0};
    AMS_CurrentStats_t out = {0};

    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc, -80000, &out)); /* peak: -80 A */
    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc, -10000, &out)); /* milder: -10 A */

    TEST_ASSERT_EQUAL_UINT32(80000, out.ui32_session_max_discharge_mA);
}

/* =========================================================================
 * SeedLifetime: lifetime = MAX(baseline, session) — a session that never
 * beats the seeded baseline must report the baseline unchanged.
 * ========================================================================= */

void test_Fold_session_never_beats_seeded_baseline(void)
{
    AMS_CurrentAccumulator_t acc = {0};
    vd_CurrentCalc_SeedLifetime(&acc, 150000); /* all-time record: 150 A discharge */

    AMS_CurrentStats_t out = {0};
    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc, -60000, &out)); /* this session only hits 60 A */

    TEST_ASSERT_EQUAL_UINT32(60000,  out.ui32_session_max_discharge_mA);
    TEST_ASSERT_EQUAL_UINT32(150000, out.ui32_lifetime_max_discharge_mA);
}

/* =========================================================================
 * SeedLifetime: a session that BEATS the seeded baseline must report the
 * new, higher session value as the lifetime max.
 * ========================================================================= */

void test_Fold_session_beats_seeded_baseline_updates_lifetime(void)
{
    AMS_CurrentAccumulator_t acc = {0};
    vd_CurrentCalc_SeedLifetime(&acc, 50000); /* old record: 50 A */

    AMS_CurrentStats_t out = {0};
    TEST_ASSERT_TRUE(b_CurrentCalc_Fold(&acc, -90000, &out)); /* new personal best: 90 A */

    TEST_ASSERT_EQUAL_UINT32(90000, out.ui32_lifetime_max_discharge_mA);
}

/* =========================================================================
 * NULL-pointer safety.
 * ========================================================================= */

void test_Fold_null_pointers_return_false_and_do_not_crash(void)
{
    AMS_CurrentAccumulator_t acc = {0};
    AMS_CurrentStats_t out = {0};

    TEST_ASSERT_FALSE(b_CurrentCalc_Fold(NULL, -1000, &out));
    TEST_ASSERT_FALSE(b_CurrentCalc_Fold(&acc, -1000, NULL));

    vd_CurrentCalc_SeedLifetime(NULL, 100); /* must not crash */
}
