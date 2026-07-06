/**
 * @file    test_AMS_gps_algorithms.c
 * @brief   Unit tests for Algorithms/AMS_gps_algorithms.c — pure, no mocks
 *          needed (no hardware/RTOS dependency).
 */

#include "unity.h"
#include "Algorithms/AMS_gps_algorithms.h"

TEST_SOURCE_FILE("Algorithms/AMS_gps_algorithms.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * st_GPS_ApplyRmcFrame
 * ========================================================================= */

/* Known-value conversion: 43 deg 12.345 min -> 43.205750 deg (x1,000,000).
 * 12.345 / 60 = 0.205750 exactly, chosen so the division is exact and the
 * test isn't sensitive to integer-truncation edge cases. */
void test_ApplyRmcFrame_valid_fix_converts_latitude_correctly(void)
{
    GPS_Data_t current = {0};
    struct minmea_sentence_rmc frame = {0};
    frame.valid = true;
    frame.latitude.value = 4312345;
    frame.latitude.scale = 1000;   /* DDMM.MMM: 4312.345 */

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 1000);

    TEST_ASSERT_EQUAL_INT32(43205750, out.i32_latitude_udeg);
}

void test_ApplyRmcFrame_valid_fix_converts_longitude_correctly(void)
{
    GPS_Data_t current = {0};
    struct minmea_sentence_rmc frame = {0};
    frame.valid = true;
    frame.longitude.value = 200000;
    frame.longitude.scale = 1000;  /* DDDMM.MMMM: 200.000 -> 2 deg 0.000 min */

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 1000);

    TEST_ASSERT_EQUAL_INT32(2000000, out.i32_longitude_udeg);
}

/* Speed: knots -> km/h -> m/s. 5.2 knots (value=52, scale=10) chosen so
 * every downstream division lands on an exact integer, no rounding. */
void test_ApplyRmcFrame_valid_fix_converts_speed_correctly(void)
{
    GPS_Data_t current = {0};
    struct minmea_sentence_rmc frame = {0};
    frame.valid = true;
    frame.speed.value = 52;
    frame.speed.scale = 10;  /* 5.2 knots */

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 1000);

    TEST_ASSERT_EQUAL_INT32(5200, out.i32_vel_knots_x1000);
    TEST_ASSERT_EQUAL_INT32(9630, out.i32_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(2675, out.i32_vel_ms_x1000);
}

void test_ApplyRmcFrame_valid_fix_sets_time_and_tick(void)
{
    GPS_Data_t current = {0};
    struct minmea_sentence_rmc frame = {0};
    frame.valid = true;
    frame.time.hours   = 12;
    frame.time.minutes = 34;
    frame.time.seconds = 56;

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 999000);

    TEST_ASSERT_EQUAL_UINT8(12, out.ui8_hour);
    TEST_ASSERT_EQUAL_UINT8(34, out.ui8_minute);
    TEST_ASSERT_EQUAL_UINT8(56, out.ui8_second);
    TEST_ASSERT_EQUAL_UINT32(999000, out.ui32_last_fix_tick_ms);
    TEST_ASSERT_TRUE(out.b_gps_is_connected);
}

/* Invalid frame: only b_gps_is_connected changes. Position/speed/time must
 * be left exactly as they were — never overwrite a good fix with garbage
 * from a sentence that failed its own checksum/validity check. */
void test_ApplyRmcFrame_invalid_fix_only_clears_connected_flag(void)
{
    GPS_Data_t current = {0};
    current.b_gps_is_connected = true;
    current.i32_latitude_udeg  = 111;
    current.i32_longitude_udeg = 222;
    current.i32_vel_kmh_x1000  = 333;
    current.ui8_hour           = 11;

    struct minmea_sentence_rmc frame = {0};
    frame.valid = false;

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 5000);

    TEST_ASSERT_FALSE(out.b_gps_is_connected);
    TEST_ASSERT_EQUAL_INT32(111, out.i32_latitude_udeg);
    TEST_ASSERT_EQUAL_INT32(222, out.i32_longitude_udeg);
    TEST_ASSERT_EQUAL_INT32(333, out.i32_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_UINT8(11, out.ui8_hour);
}

/* Guard: scale == 0 means "no data" in minmea's convention — must not
 * divide by zero, and must leave the field untouched (matches the `if
 * (scale != 0)` guard in the source). */
void test_ApplyRmcFrame_zero_scale_leaves_latitude_untouched(void)
{
    GPS_Data_t current = {0};
    current.i32_latitude_udeg = 42;

    struct minmea_sentence_rmc frame = {0};
    frame.valid = true;
    frame.latitude.value = 12345;
    frame.latitude.scale = 0; /* "unknown" per minmea convention */

    GPS_Data_t out = st_GPS_ApplyRmcFrame(current, &frame, 1000);

    TEST_ASSERT_EQUAL_INT32(42, out.i32_latitude_udeg);
}

/* =========================================================================
 * st_GPS_ApplyGgaFrame
 * ========================================================================= */

void test_ApplyGgaFrame_updates_fix_quality_and_satellites_only(void)
{
    GPS_Data_t current = {0};
    current.i32_latitude_udeg = 999; /* must survive untouched */

    struct minmea_sentence_gga frame = {0};
    frame.fix_quality        = 2;
    frame.satellites_tracked = 9;

    GPS_Data_t out = st_GPS_ApplyGgaFrame(current, &frame);

    TEST_ASSERT_EQUAL_UINT8(2, out.ui8_fix_quality);
    TEST_ASSERT_EQUAL_UINT8(9, out.ui8_satellites);
    TEST_ASSERT_EQUAL_INT32(999, out.i32_latitude_udeg);
}
