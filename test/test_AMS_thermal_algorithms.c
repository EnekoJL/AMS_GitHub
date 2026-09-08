/**
 * @file    test_AMS_thermal_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_thermal_algorithms.c — pure, no
 *          mocks needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_thermal_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_thermal_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Basic spatial stats: max/min/avg/delta and which cell each belongs to,
 * from a single reading.
 * ========================================================================= */

void test_Fold_computes_spatial_stats_from_one_reading(void)
{
    AMS_ThermalAccumulator_t acc = {0};
    int16_t cells[4] = { 2500, 3100, 2800, 2200 }; /* centi-C: 25.00, 31.00, 28.00, 22.00 */
    AMS_ThermalStats_t out = {0};

    TEST_ASSERT_TRUE(b_ThermalCalc_Fold(&acc, cells, 4, &out));

    TEST_ASSERT_EQUAL_INT16(3100, out.i16_max_cell_temp_cC);
    TEST_ASSERT_EQUAL_INT16(2200, out.i16_min_cell_temp_cC);
    TEST_ASSERT_EQUAL_INT16(2650, out.i16_avg_cell_temp_cC); /* (2500+3100+2800+2200)/4 */
    TEST_ASSERT_EQUAL_INT16(900,  out.i16_delta_temp_cC);    /* 3100-2200 */
    TEST_ASSERT_EQUAL_UINT8(1, out.ui8_hottest_cell_id);
    TEST_ASSERT_EQUAL_UINT8(3, out.ui8_coldest_cell_id);
}

/* =========================================================================
 * Session worst-case must persist across readings — a milder second
 * reading must not overwrite the session max temp/delta seen earlier.
 * ========================================================================= */

void test_Fold_session_max_persists_across_milder_readings(void)
{
    AMS_ThermalAccumulator_t acc = {0};
    AMS_ThermalStats_t out = {0};

    int16_t hot[2]  = { 4500, 3000 }; /* delta 1500, max 4500 */
    int16_t mild[2] = { 2600, 2500 }; /* delta 100,  max 2600 */

    TEST_ASSERT_TRUE(b_ThermalCalc_Fold(&acc, hot, 2, &out));
    TEST_ASSERT_EQUAL_INT16(4500, out.i16_session_max_temp_cC);
    TEST_ASSERT_EQUAL_INT16(1500, out.i16_session_max_delta_cC);

    TEST_ASSERT_TRUE(b_ThermalCalc_Fold(&acc, mild, 2, &out));
    /* This-sample fields reflect the milder reading... */
    TEST_ASSERT_EQUAL_INT16(2600, out.i16_max_cell_temp_cC);
    /* ...but session worst-case must still remember the earlier hot reading */
    TEST_ASSERT_EQUAL_INT16(4500, out.i16_session_max_temp_cC);
    TEST_ASSERT_EQUAL_INT16(1500, out.i16_session_max_delta_cC);
}

/* =========================================================================
 * Session average is a TIME-average of the spatial mean, not the spatial
 * mean itself — two readings with different spatial means must blend.
 * ========================================================================= */

void test_Fold_session_avg_is_time_average_of_spatial_mean(void)
{
    AMS_ThermalAccumulator_t acc = {0};
    AMS_ThermalStats_t out = {0};

    int16_t reading_a[1] = { 2000 }; /* spatial mean 2000 */
    int16_t reading_b[1] = { 4000 }; /* spatial mean 4000 */

    TEST_ASSERT_TRUE(b_ThermalCalc_Fold(&acc, reading_a, 1, &out));
    TEST_ASSERT_EQUAL_INT16(2000, out.i16_session_avg_temp_cC);

    TEST_ASSERT_TRUE(b_ThermalCalc_Fold(&acc, reading_b, 1, &out));
    TEST_ASSERT_EQUAL_INT16(3000, out.i16_session_avg_temp_cC); /* (2000+4000)/2 samples */
}

/* =========================================================================
 * NULL / zero-count safety.
 * ========================================================================= */

void test_Fold_null_or_zero_count_returns_false_and_do_not_crash(void)
{
    AMS_ThermalAccumulator_t acc = {0};
    int16_t cells[1] = { 2500 };
    AMS_ThermalStats_t out = {0};

    TEST_ASSERT_FALSE(b_ThermalCalc_Fold(NULL, cells, 1, &out));
    TEST_ASSERT_FALSE(b_ThermalCalc_Fold(&acc, NULL, 1, &out));
    TEST_ASSERT_FALSE(b_ThermalCalc_Fold(&acc, cells, 0, &out));
    TEST_ASSERT_FALSE(b_ThermalCalc_Fold(&acc, cells, 1, NULL));
}
