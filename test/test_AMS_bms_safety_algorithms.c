/**
 * @file    test_AMS_bms_safety_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_bms_safety_algorithms.c — pure, no
 *          mocks needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_bms_safety_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_bms_safety_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * Cell voltage debounce
 * ========================================================================= */

void test_CheckCellVoltage_all_in_range_never_faults(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t cells[4] = {3300, 3400, 3350, 3200};

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, cells, 4,
                                                          BMS_CELL_UNDERVOLTAGE_mV,
                                                          BMS_CELL_OVERVOLTAGE_mV));
    }
}

void test_CheckCellVoltage_single_bad_sample_does_not_latch(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t bad[4]  = {3300, 2700, 3350, 3200}; /* cell 1 under threshold */
    uint16_t good[4] = {3300, 3400, 3350, 3200};

    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, bad, 4,
                                                      BMS_CELL_UNDERVOLTAGE_mV,
                                                      BMS_CELL_OVERVOLTAGE_mV));
    /* Recovers before reaching the debounce depth */
    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, good, 4,
                                                      BMS_CELL_UNDERVOLTAGE_mV,
                                                      BMS_CELL_OVERVOLTAGE_mV));
}

void test_CheckCellVoltage_faults_after_debounce_depth_consecutive_bad_samples(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t under[4] = {3300, 2700, 3350, 3200}; /* cell 1 = 2700mV, below 2800mV UV */

    for (uint8_t i = 0; i < (BMS_FAULT_DEBOUNCE_SAMPLES - 1u); i++) {
        TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, under, 4,
                                                          BMS_CELL_UNDERVOLTAGE_mV,
                                                          BMS_CELL_OVERVOLTAGE_mV));
    }
    /* The Nth consecutive bad sample latches the fault */
    TEST_ASSERT_TRUE(b_BmsSafety_CheckCellVoltage(&state, under, 4,
                                                     BMS_CELL_UNDERVOLTAGE_mV,
                                                     BMS_CELL_OVERVOLTAGE_mV));
}

void test_CheckCellVoltage_overvoltage_also_faults(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t over[3] = {3300, 3400, 4350}; /* cell 2 above 4300mV OV */

    for (uint8_t i = 0; i < BMS_FAULT_DEBOUNCE_SAMPLES; i++) {
        b_BmsSafety_CheckCellVoltage(&state, over, 3, BMS_CELL_UNDERVOLTAGE_mV, BMS_CELL_OVERVOLTAGE_mV);
    }
    TEST_ASSERT_TRUE(b_BmsSafety_CheckCellVoltage(&state, over, 3,
                                                     BMS_CELL_UNDERVOLTAGE_mV,
                                                     BMS_CELL_OVERVOLTAGE_mV));
}

void test_CheckCellVoltage_recovering_clears_a_latched_fault(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t under[2] = {2700, 3300};
    uint16_t good[2]  = {3300, 3300};

    for (uint8_t i = 0; i < (BMS_FAULT_DEBOUNCE_SAMPLES + 1u); i++) {
        b_BmsSafety_CheckCellVoltage(&state, under, 2, BMS_CELL_UNDERVOLTAGE_mV, BMS_CELL_OVERVOLTAGE_mV);
    }
    /* One good sample resets the debounce counter to zero */
    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, good, 2,
                                                      BMS_CELL_UNDERVOLTAGE_mV,
                                                      BMS_CELL_OVERVOLTAGE_mV));
}

void test_CheckCellVoltage_null_args_return_false(void)
{
    AMS_CellVoltageFaultState_t state = {0};
    uint16_t cells[1] = {3300};

    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(NULL, cells, 1, 2800, 4300));
    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, NULL, 1, 2800, 4300));
    TEST_ASSERT_FALSE(b_BmsSafety_CheckCellVoltage(&state, cells, 0, 2800, 4300));
}

/* =========================================================================
 * NTC voltage -> temperature lookup
 * ========================================================================= */

void test_NtcVoltageToTempC_25C_reference_point(void)
{
    /* At 25C, table interpolates near ~10450.4 ohm (24C) / 9571.5 ohm (26C).
     * Reference divider: Vref=3.0V, pullup=10k. Solve for the NTC voltage
     * that produces ~10000 ohm (close to the 25C midpoint) and check the
     * result lands in a sane band, since the table doesn't have an exact
     * 25C entry (steps are even degrees only: 24, 26). */
    float v = (3.0f * 10000.0f) / (10000.0f + 10000.0f); /* r_ntc = 10000 ohm -> v = 1.5V */
    float t = f_BmsSafety_NtcVoltageToTempC(v);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, t);
}

void test_NtcVoltageToTempC_out_of_range_returns_error_sentinel(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-273.15f, f_BmsSafety_NtcVoltageToTempC(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(-273.15f, f_BmsSafety_NtcVoltageToTempC(3.0f));
    TEST_ASSERT_EQUAL_FLOAT(-273.15f, f_BmsSafety_NtcVoltageToTempC(-1.0f));
}

void test_NtcVoltageToTempC_clamps_at_table_hottest_entry(void)
{
    /* r_ntc = pullup*v/(Vref-v) grows with v, so LOW voltage -> LOW
     * resistance -> clamp to the table's HOTTEST entry (100C). */
    float t = f_BmsSafety_NtcVoltageToTempC(0.01f);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, t);
}

void test_NtcVoltageToTempC_clamps_at_table_coldest_entry(void)
{
    /* HIGH voltage (close to Vref) -> HIGH resistance -> clamp to the
     * table's COLDEST entry (-40C). */
    float t = f_BmsSafety_NtcVoltageToTempC(2.99f);
    TEST_ASSERT_EQUAL_FLOAT(-40.0f, t);
}
