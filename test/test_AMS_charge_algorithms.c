/**
 * @file    test_AMS_charge_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_charge_algorithms.c — pure, no
 *          mocks needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_charge_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_charge_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * First sample: nothing to integrate yet (no dt) — must return false and
 * leave the output untouched.
 * ========================================================================= */

void test_Fold_first_sample_returns_false_no_dt_yet(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    AMS_ChargeStats_t out = {0};
    out.ui32_session_discharged_mAh = 999; /* sentinel */

    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 1000, &out));
    TEST_ASSERT_EQUAL_UINT32(999, out.ui32_session_discharged_mAh);
}

/* =========================================================================
 * Duplicate tick (dt == 0) must also return false, not double-integrate.
 * ========================================================================= */

void test_Fold_duplicate_tick_returns_false(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    AMS_ChargeStats_t out = {0};

    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 1000, &out)); /* primes */
    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 1000, &out)); /* same tick again */
}

/* =========================================================================
 * Exact integration: constant -1000 mA (1 A discharge) for 3600000 ms (1 h)
 * must be exactly 1000 mAh discharged, 0 charged.
 * ========================================================================= */

void test_Fold_constant_discharge_for_one_hour_is_exact(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    AMS_ChargeStats_t out = {0};

    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 0, &out)); /* prime at t=0 */
    TEST_ASSERT_TRUE(b_ChargeCalc_Fold(&acc, -1000, 3600000, &out));

    TEST_ASSERT_EQUAL_UINT32(1000, out.ui32_session_discharged_mAh);
    TEST_ASSERT_EQUAL_UINT32(0,    out.ui32_session_charged_mAh);
    TEST_ASSERT_EQUAL_UINT32(1000, out.ui32_lifetime_discharged_mAh); /* baseline 0 */
}

/* =========================================================================
 * Charge and discharge must accumulate into separate counters — a
 * discharge sample followed by a charge sample must not net them out.
 * ========================================================================= */

void test_Fold_charge_and_discharge_accumulate_separately(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    AMS_ChargeStats_t out = {0};

    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -2000, 0, &out));       /* prime */
    TEST_ASSERT_TRUE(b_ChargeCalc_Fold(&acc, -2000, 1800000, &out));  /* -2A for 0.5h -> 1000 mAh discharged */
    TEST_ASSERT_TRUE(b_ChargeCalc_Fold(&acc, 1000, 3600000, &out));   /* +1A for 0.5h -> 500 mAh charged */

    TEST_ASSERT_EQUAL_UINT32(1000, out.ui32_session_discharged_mAh);
    TEST_ASSERT_EQUAL_UINT32(500,  out.ui32_session_charged_mAh);
}

/* =========================================================================
 * SeedLifetime: lifetime = baseline + session, and the baseline must not
 * leak into the session-only fields.
 * ========================================================================= */

void test_Fold_seeded_lifetime_adds_baseline(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    vd_ChargeCalc_SeedLifetime(&acc, 500000, 480000); /* 500 Ah / 480 Ah on the odometer */

    AMS_ChargeStats_t out = {0};
    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 0, &out));
    TEST_ASSERT_TRUE(b_ChargeCalc_Fold(&acc, -1000, 3600000, &out)); /* +1000 mAh this session */

    TEST_ASSERT_EQUAL_UINT32(1000,   out.ui32_session_discharged_mAh);
    TEST_ASSERT_EQUAL_UINT32(501000, out.ui32_lifetime_discharged_mAh);
    TEST_ASSERT_EQUAL_UINT32(480000, out.ui32_lifetime_charged_mAh); /* untouched this session */
}

/* =========================================================================
 * NULL-pointer safety.
 * ========================================================================= */

void test_Fold_null_pointers_return_false_and_do_not_crash(void)
{
    AMS_ChargeAccumulator_t acc = {0};
    AMS_ChargeStats_t out = {0};

    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(NULL, -1000, 1000, &out));
    TEST_ASSERT_FALSE(b_ChargeCalc_Fold(&acc, -1000, 1000, NULL));

    vd_ChargeCalc_SeedLifetime(NULL, 100, 100); /* must not crash */
}
