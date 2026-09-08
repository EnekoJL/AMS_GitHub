/**
 * @file    test_AMS_DataBroker.c
 * @brief   Unit tests for the Data Broker (Core/Src/Middleware/AMS_DataBroker.c).
 *
 *          cmsis_os.h is mocked (CMock) so this runs on a host with no real
 *          RTOS. Mocking a real mutex means WE control exactly when it
 *          "succeeds" or "times out" — this is what lets us test the
 *          50ms-timeout/fault-counter behaviour without a real stuck task.
 *
 *          IMPORTANT — TEST ORDER MATTERS IN THIS FILE.
 *          The Broker keeps its real state (mutex handles, data, the fault
 *          counter, freshness timestamps) in file-scope statics that persist
 *          for the whole test binary's run — Unity does not restart the
 *          process between test functions. So:
 *            - The "before init" test MUST run first (it needs is_initialized
 *              to still be false).
 *            - The "Init creates six mutexes" test MUST run before any test
 *              that exercises a Get/Update (they need real mutex handles to
 *              have been created), and it only runs osMutexNew() the FIRST
 *              time — a second call to b_Broker_Init() later would not
 *              re-create mutexes, since the code only calls osMutexNew() if
 *              the static handle is still NULL.
 *          Unity runs test functions in the order they're declared in this
 *          file (not alphabetically), so keep that order intact.
 */

#include "unity.h"
#include "mock_cmsis_os.h"
#include "Middleware/AMS_DataBroker.h"
#include "AMS_DataStructs.h"

TEST_SOURCE_FILE("Middleware/AMS_DataBroker.c")

/* Fake mutex "handles" — osMutexId_t is just a void*, never dereferenced by
 * the mock, so any distinct non-NULL values work. One per domain lets each
 * test assert the Broker acquired the CORRECT domain's mutex, not just *a*
 * mutex — this is the ownership-table promise from Architecture_Overview.md
 * being checked in code. */
#define FAKE_MTX_VEH  ((osMutexId_t)0x1001)
#define FAKE_MTX_ADC  ((osMutexId_t)0x1002)
#define FAKE_MTX_PERS ((osMutexId_t)0x1003)
#define FAKE_MTX_GPS  ((osMutexId_t)0x1004)
#define FAKE_MTX_TEL  ((osMutexId_t)0x1005)
#define FAKE_MTX_BMS  ((osMutexId_t)0x1006)
#define FAKE_MTX_PWR  ((osMutexId_t)0x1007)
#define FAKE_MTX_STATS ((osMutexId_t)0x1008)

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * T00 — before b_Broker_Init(), every Get/Update must fail closed.
 * No mocks expected: the NULL/!initialized guard returns before ever
 * touching a mutex, so if this test unexpectedly calls into cmsis_os,
 * CMock will fail it for an unexpected call — that itself is a useful
 * assertion that the guard is actually first in the function.
 * ========================================================================= */
void test_T00_before_init_every_function_fails_closed(void)
{
    Vehicle_Data_t       veh    = {0};
    AMS_ADC_Data_t       adc    = {0};
    GPS_Data_t           gps    = {0};
    AMS_Telemetry_Data_t telem  = {0};
    AMS_BMS_Data_t       bms    = {0};
    AMS_Powertrain_Data_t pt    = {0};
    AMS_BatteryStats_Data_t stats = {0};
    AMS_Persistent_Config_t cfg = {0};
    AMS_Safety_Flags_t   flags  = {0};
    AMS_Data_t           all    = {0};

    TEST_ASSERT_FALSE(b_Broker_Get_VehicleState(&veh));
    TEST_ASSERT_FALSE(b_Broker_Update_VehicleState(&veh));
    TEST_ASSERT_FALSE(b_Broker_Get_ADCData(&adc));
    TEST_ASSERT_FALSE(b_Broker_Update_ADCData(&adc));
    TEST_ASSERT_FALSE(b_Broker_Get_GPSData(&gps));
    TEST_ASSERT_FALSE(b_Broker_Update_GPSData(&gps));
    TEST_ASSERT_FALSE(b_Broker_Get_TelemetryData(&telem));
    TEST_ASSERT_FALSE(b_Broker_Update_TelemetryData(&telem));
    TEST_ASSERT_FALSE(b_Broker_Get_BMSData(&bms));
    TEST_ASSERT_FALSE(b_Broker_Update_BMSData(&bms));
    TEST_ASSERT_FALSE(b_Broker_Get_PowertrainData(&pt));
    TEST_ASSERT_FALSE(b_Broker_Update_PowertrainData(&pt));
    TEST_ASSERT_FALSE(b_Broker_Get_BatteryStats(&stats));
    TEST_ASSERT_FALSE(b_Broker_Update_BatteryStats(&stats));
    TEST_ASSERT_FALSE(b_Broker_Get_PersistentConfig(&cfg));
    vd_Broker_Set_PersistentConfig(&cfg); /* void — must not crash */
    TEST_ASSERT_FALSE(b_Broker_Get_SafetyFlags(&flags));
    TEST_ASSERT_FALSE(b_Broker_Get_AllData(&all));

    TEST_ASSERT_EQUAL_UINT32(0, u32_Broker_GetFaultCount());
}

/* =========================================================================
 * T01 — b_Broker_Init() creates exactly eight mutexes, in this fixed order:
 * vehiculo, adc, persistent, gps, telemetry, bms, powertrain, battery_stats
 * (matches the source).
 * ========================================================================= */
void test_T01_init_creates_eight_mutexes_in_order(void)
{
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_VEH);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_ADC);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_PERS);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_GPS);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_TEL);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_BMS);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_PWR);
    osMutexNew_ExpectAnyArgsAndReturn(FAKE_MTX_STATS);

    b_Broker_Init();
}

/* =========================================================================
 * T02 — immediately after Init, nothing has been written yet, so every
 * domain must report stale (b_..._fresh == false) — including BMS, which
 * has no producer task at all yet (see Architecture_Overview.md).
 * ========================================================================= */
void test_T02_safety_flags_all_stale_immediately_after_init(void)
{
    osKernelGetTickCount_ExpectAndReturn(5000);

    AMS_Safety_Flags_t flags = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_SafetyFlags(&flags));

    TEST_ASSERT_FALSE(flags.b_vehicle_data_fresh);
    TEST_ASSERT_FALSE(flags.b_adc_data_fresh);
    TEST_ASSERT_FALSE(flags.b_gps_data_fresh);
    TEST_ASSERT_FALSE(flags.b_bms_data_fresh);
    TEST_ASSERT_FALSE(flags.b_telemetry_data_fresh);
    TEST_ASSERT_FALSE(flags.b_powertrain_data_fresh);
    TEST_ASSERT_FALSE(flags.b_battery_stats_fresh);
}

/* =========================================================================
 * T03 — Vehicle roundtrip: Update() then Get() must see the same data,
 * using the vehicle mutex specifically (not any other domain's).
 * ========================================================================= */
void test_T03_vehicle_roundtrip_uses_vehicle_mutex(void)
{
    Vehicle_Data_t sent = {0};
    sent.bateria_12v_mV       = 12500;
    sent.recorrido_susp_1_dmm = 1452;
    sent.recorrido_susp_2_dmm = 485;

    /* Update: Acquire -> memcpy -> stamp timestamp -> Release */
    osMutexAcquire_ExpectAndReturn(FAKE_MTX_VEH, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(1000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_VEH, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_VehicleState(&sent));

    /* Get: Acquire -> memcpy -> Release (no timestamp involved) */
    osMutexAcquire_ExpectAndReturn(FAKE_MTX_VEH, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_VEH, osOK);

    Vehicle_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_VehicleState(&got));

    TEST_ASSERT_EQUAL_UINT32(sent.bateria_12v_mV, got.bateria_12v_mV);
    TEST_ASSERT_EQUAL_UINT32(sent.recorrido_susp_1_dmm, got.recorrido_susp_1_dmm);
    TEST_ASSERT_EQUAL_UINT32(sent.recorrido_susp_2_dmm, got.recorrido_susp_2_dmm);
}

/* =========================================================================
 * T04 — After T03 stamped the vehicle domain at tick 1000, querying safety
 * flags "now" at tick 1100 (100ms later, under the 300ms threshold) must
 * report vehicle fresh while every other still-untouched domain stays stale.
 * ========================================================================= */
void test_T04_safety_flags_vehicle_fresh_others_still_stale(void)
{
    osKernelGetTickCount_ExpectAndReturn(1100);

    AMS_Safety_Flags_t flags = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_SafetyFlags(&flags));

    TEST_ASSERT_TRUE(flags.b_vehicle_data_fresh);
    TEST_ASSERT_FALSE(flags.b_adc_data_fresh);
    TEST_ASSERT_FALSE(flags.b_gps_data_fresh);
    TEST_ASSERT_FALSE(flags.b_bms_data_fresh);
    TEST_ASSERT_FALSE(flags.b_telemetry_data_fresh);
    TEST_ASSERT_FALSE(flags.b_powertrain_data_fresh);
    TEST_ASSERT_FALSE(flags.b_battery_stats_fresh);
}

/* =========================================================================
 * T05 — GPS roundtrip, same shape as T03 but on the gps mutex.
 * ========================================================================= */
void test_T05_gps_roundtrip_uses_gps_mutex(void)
{
    GPS_Data_t sent = {0};
    sent.b_gps_is_connected = true;
    sent.ui8_satellites     = 9;
    sent.i32_latitude_udeg  = 43123456;
    sent.i32_longitude_udeg = 2987654;
    sent.i32_vel_kmh_x1000  = 45000;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_GPS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(2000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_GPS, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_GPSData(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_GPS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_GPS, osOK);

    GPS_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_GPSData(&got));

    TEST_ASSERT_TRUE(got.b_gps_is_connected);
    TEST_ASSERT_EQUAL_UINT8(9, got.ui8_satellites);
    TEST_ASSERT_EQUAL_INT32(43123456, got.i32_latitude_udeg);
    TEST_ASSERT_EQUAL_INT32(2987654, got.i32_longitude_udeg);
    TEST_ASSERT_EQUAL_INT32(45000, got.i32_vel_kmh_x1000);
}

/* =========================================================================
 * T06 — Telemetry roundtrip.
 * ========================================================================= */
void test_T06_telemetry_roundtrip_uses_telemetry_mutex(void)
{
    AMS_Telemetry_Data_t sent = {0};
    sent.ui32_total_distance_m    = 1200;
    sent.i32_max_vel_kmh_x1000    = 88000;
    sent.i32_avg_vel_kmh_x1000    = 42000;
    sent.i32_max_accel_ms2_x1000  = 3200;
    sent.i32_max_decel_ms2_x1000  = -5100;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_TEL, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(3000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_TEL, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_TelemetryData(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_TEL, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_TEL, osOK);

    AMS_Telemetry_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_TelemetryData(&got));

    TEST_ASSERT_EQUAL_UINT32(1200, got.ui32_total_distance_m);
    TEST_ASSERT_EQUAL_INT32(88000, got.i32_max_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(42000, got.i32_avg_vel_kmh_x1000);
    TEST_ASSERT_EQUAL_INT32(3200, got.i32_max_accel_ms2_x1000);
    TEST_ASSERT_EQUAL_INT32(-5100, got.i32_max_decel_ms2_x1000);
}

/* =========================================================================
 * T07 — BMS roundtrip (placeholder domain — no real producer task exists
 * yet, but the Broker storage/API itself must work correctly today).
 * ========================================================================= */
void test_T07_bms_roundtrip_uses_bms_mutex(void)
{
    AMS_BMS_Data_t sent = {0};
    sent.ui32_pack_voltage_mV = 98000;
    sent.i32_pack_current_mA  = -1500;
    sent.ui16_min_cell_mV     = 3350;
    sent.ui16_max_cell_mV     = 3410;
    sent.i16_max_cell_temp_cC = 3520;
    sent.soc_percent_x10      = 812;
    sent.ui32_fault_flags     = 0;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_BMS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(4000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_BMS, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_BMSData(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_BMS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_BMS, osOK);

    AMS_BMS_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_BMSData(&got));

    TEST_ASSERT_EQUAL_UINT32(98000, got.ui32_pack_voltage_mV);
    TEST_ASSERT_EQUAL_INT32(-1500, got.i32_pack_current_mA);
    TEST_ASSERT_EQUAL_UINT16(3350, got.ui16_min_cell_mV);
    TEST_ASSERT_EQUAL_UINT16(3410, got.ui16_max_cell_mV);
    TEST_ASSERT_EQUAL_INT16(3520, got.i16_max_cell_temp_cC);
    TEST_ASSERT_EQUAL_UINT16(812, got.soc_percent_x10);
}

/* =========================================================================
 * T08 — ADC roundtrip.
 * ========================================================================= */
void test_T08_adc_roundtrip_uses_adc_mutex(void)
{
    AMS_ADC_Data_t sent = {0};
    sent.voltaje_adc1_mV     = 12300;
    sent.voltaje_adc2_ch1_mV = 4500;
    sent.voltaje_adc2_ch2_mV = 1200;
    sent.i32_mcu_temp_cC     = 2547;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_ADC, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(6000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_ADC, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_ADCData(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_ADC, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_ADC, osOK);

    AMS_ADC_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_ADCData(&got));

    TEST_ASSERT_EQUAL_UINT32(12300, got.voltaje_adc1_mV);
    TEST_ASSERT_EQUAL_UINT32(4500, got.voltaje_adc2_ch1_mV);
    TEST_ASSERT_EQUAL_UINT32(1200, got.voltaje_adc2_ch2_mV);
    TEST_ASSERT_EQUAL_INT32(2547, got.i32_mcu_temp_cC);
}

/* =========================================================================
 * T08b — Powertrain roundtrip (own mutex, split out of Vehicle_Data_t to
 * fix the ADC/CAN lost-update race — see Architecture_Overview.md Section 4).
 * ========================================================================= */
void test_T08b_powertrain_roundtrip_uses_powertrain_mutex(void)
{
    AMS_Powertrain_Data_t sent = {0};
    sent.inverter_rpm = -1500;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_PWR, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(7000);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_PWR, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_PowertrainData(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_PWR, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_PWR, osOK);

    AMS_Powertrain_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_PowertrainData(&got));

    TEST_ASSERT_EQUAL_INT16(-1500, got.inverter_rpm);
}

/* =========================================================================
 * T08c — Battery stats roundtrip (own mutex; composes AMS_ChargeStats_t /
 * AMS_ThermalStats_t / AMS_CurrentStats_t — see AMS_DataStructs.h).
 * ========================================================================= */
void test_T08c_battery_stats_roundtrip_uses_battery_stats_mutex(void)
{
    AMS_BatteryStats_Data_t sent = {0};
    sent.charge.ui32_session_discharged_mAh  = 1500;
    sent.thermal.i16_max_cell_temp_cC        = 4200;
    sent.current.ui32_session_max_discharge_mA = 90000;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_STATS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osKernelGetTickCount_ExpectAndReturn(7500);
    osMutexRelease_ExpectAndReturn(FAKE_MTX_STATS, osOK);

    TEST_ASSERT_TRUE(b_Broker_Update_BatteryStats(&sent));

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_STATS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_STATS, osOK);

    AMS_BatteryStats_Data_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_BatteryStats(&got));

    TEST_ASSERT_EQUAL_UINT32(1500,  got.charge.ui32_session_discharged_mAh);
    TEST_ASSERT_EQUAL_INT16(4200,   got.thermal.i16_max_cell_temp_cC);
    TEST_ASSERT_EQUAL_UINT32(90000, got.current.ui32_session_max_discharge_mA);
}

/* =========================================================================
 * T09 — Persistent config roundtrip. Unlike every other domain, this one
 * does NOT participate in freshness tracking (no timestamp stamped, not in
 * AMS_Safety_Flags_t) — it's Flash-backed config, not real-time telemetry.
 * ========================================================================= */
void test_T09_persistent_config_roundtrip_uses_persistent_mutex(void)
{
    AMS_Persistent_Config_t sent = {0};
    sent.soc_percent_x10 = 955;
    sent.cycle_count     = 42;

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_PERS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_PERS, osOK);

    vd_Broker_Set_PersistentConfig(&sent); /* void */

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_PERS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_PERS, osOK);

    AMS_Persistent_Config_t got = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_PersistentConfig(&got));

    TEST_ASSERT_EQUAL_UINT16(955, got.soc_percent_x10);
    TEST_ASSERT_EQUAL_UINT16(42, got.cycle_count);
}

/* =========================================================================
 * T10 — A mutex timeout on a Get must: return false, NOT call Release
 * (never acquired it), and increment the fault counter by exactly one.
 * ========================================================================= */
void test_T10_get_mutex_timeout_returns_false_and_counts_fault(void)
{
    uint32_t before = u32_Broker_GetFaultCount();

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_ADC, 0, osErrorTimeout);
    osMutexAcquire_IgnoreArg_timeout();
    /* No osMutexRelease expected: acquire failed, function returns early. */

    AMS_ADC_Data_t copy = {0};
    TEST_ASSERT_FALSE(b_Broker_Get_ADCData(&copy));

    TEST_ASSERT_EQUAL_UINT32(before + 1, u32_Broker_GetFaultCount());
}

/* =========================================================================
 * T11 — Same as T10, but on the write path (Update), on a different domain.
 * ========================================================================= */
void test_T11_update_mutex_timeout_returns_false_and_counts_fault(void)
{
    uint32_t before = u32_Broker_GetFaultCount();

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_TEL, 0, osErrorTimeout);
    osMutexAcquire_IgnoreArg_timeout();

    AMS_Telemetry_Data_t data = {0};
    TEST_ASSERT_FALSE(b_Broker_Update_TelemetryData(&data));

    TEST_ASSERT_EQUAL_UINT32(before + 1, u32_Broker_GetFaultCount());
}

/* =========================================================================
 * T12 — b_Broker_Get_AllData() calls every domain's Getter in this fixed
 * order: vehicle, bms, sensors(adc), gps, telemetry, powertrain,
 * battery_stats, then safety flags.
 * ========================================================================= */
void test_T12_get_all_data_visits_every_domain_in_order(void)
{
    osMutexAcquire_ExpectAndReturn(FAKE_MTX_VEH, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_VEH, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_BMS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_BMS, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_ADC, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_ADC, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_GPS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_GPS, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_TEL, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_TEL, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_PWR, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_PWR, osOK);

    osMutexAcquire_ExpectAndReturn(FAKE_MTX_STATS, 0, osOK);
    osMutexAcquire_IgnoreArg_timeout();
    osMutexRelease_ExpectAndReturn(FAKE_MTX_STATS, osOK);

    osKernelGetTickCount_ExpectAndReturn(9000);

    AMS_Data_t snap = {0};
    TEST_ASSERT_TRUE(b_Broker_Get_AllData(&snap));

    /* Vehicle data was written back in T03 and never changed since. */
    TEST_ASSERT_EQUAL_UINT32(12500, snap.vehicle.bateria_12v_mV);
    /* Powertrain data was written back in T08b and never changed since. */
    TEST_ASSERT_EQUAL_INT16(-1500, snap.powertrain.inverter_rpm);
    /* Battery stats were written back in T08c and never changed since. */
    TEST_ASSERT_EQUAL_UINT32(1500, snap.battery_stats.charge.ui32_session_discharged_mAh);
}

/* =========================================================================
 * T13 — NULL-pointer safety on every function, order-independent (the NULL
 * check runs before any mutex touch, so no mocks are expected here either).
 * ========================================================================= */
void test_T13_null_pointer_calls_do_not_crash(void)
{
    TEST_ASSERT_FALSE(b_Broker_Get_VehicleState(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_VehicleState(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_ADCData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_ADCData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_GPSData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_GPSData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_TelemetryData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_TelemetryData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_BMSData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_BMSData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_PowertrainData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_PowertrainData(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_BatteryStats(NULL));
    TEST_ASSERT_FALSE(b_Broker_Update_BatteryStats(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_PersistentConfig(NULL));
    vd_Broker_Set_PersistentConfig(NULL); /* void — must not crash */
    TEST_ASSERT_FALSE(b_Broker_Get_SafetyFlags(NULL));
    TEST_ASSERT_FALSE(b_Broker_Get_AllData(NULL));
}
