/**
 * @file    test_AMS_CAN_Task.c
 * @brief   Unit tests for parse_inverter_status() in AMS_CAN_Task.c.
 *
 *          parse_inverter_status() is `static` and its CAN_RxPacket_t
 *          argument type is defined privately inside AMS_CAN_Task.c — by
 *          design, per Architecture_Overview.md's ownership rules. Per
 *          instruction, the original source is NOT modified to make this
 *          testable (no STATIC macro, no new public getter).
 *
 *          Instead, this test file #includes the real .c file directly
 *          (not the .h) — a standard Unity/Ceedling technique. Since the
 *          preprocessor pastes the whole file's text into this translation
 *          unit, `parse_inverter_status` and `CAN_RxPacket_t` become
 *          directly visible here with zero changes to AMS_CAN_Task.c
 *          itself. C's `static` only restricts visibility ACROSS separate
 *          translation units — within the same one (which this now is),
 *          there's nothing to restrict.
 *
 *          Cost of this approach: the WHOLE file must compile as one unit,
 *          so every dependency it has — not just what parse_inverter_status
 *          touches — needs a mock or stub:
 *            - test/vendor_stubs/main.h   (new: minimal CAN/GPIO/HAL types)
 *            - test/vendor_stubs/cmsis_os.h (extended: message queue calls)
 *            - mock_AMS_Led_Task.h, mock_AMS_can_driver.h, mock_AMS_DataBroker.h
 *          None of those mocks are ever _Expect'd in these tests (this
 *          file's other functions — vd_CAN_Task_Init, vd_CAN_Manager_
 *          TaskProcess — are never called here) but the linker still needs
 *          every symbol their (uncalled) object code references to resolve.
 *
 *          DO NOT also add TEST_SOURCE_FILE("Middleware/AMS_CAN_Task.c")
 *          here — that would compile+link the real file a second time,
 *          causing duplicate-symbol link errors.
 *
 *          parse_inverter_status() writes to AMS_Powertrain_Data_t (its own
 *          Broker domain — see AMS_DataStructs.h), not Vehicle_Data_t. The
 *          whole domain is exactly the one RPM value carried by this CAN
 *          frame, so the function constructs and stores it directly —
 *          no Get-before-Set needed (unlike a partial-field update).
 */

#include "unity.h"
#include "mock_cmsis_os.h"
#include "mock_main.h"
#include "mock_AMS_Led_Task.h"
#include "mock_AMS_can_driver.h"
#include "mock_AMS_DataBroker.h"

#include "Middleware/AMS_CAN_Task.c"

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * parse_inverter_status(): RPM is Little-Endian signed 16-bit in bytes[0..1]
 * ========================================================================= */

void test_parse_inverter_status_positive_rpm(void)
{
    CAN_RxPacket_t pkt = {0};
    pkt.std_id  = 0x181;
    pkt.data[0] = 0xE8; /* LSB */
    pkt.data[1] = 0x03; /* MSB -> 0x03E8 = 1000 (positive: MSB top bit clear) */

    AMS_Powertrain_Data_t expected = { .inverter_rpm = 1000 };
    b_Broker_Update_PowertrainData_ExpectAndReturn(&expected, true);

    parse_inverter_status(&pkt);
}

void test_parse_inverter_status_negative_rpm(void)
{
    CAN_RxPacket_t pkt = {0};
    pkt.std_id  = 0x181;
    pkt.data[0] = 0x18; /* LSB */
    pkt.data[1] = 0xFC; /* MSB -> 0xFC18 = -1000 (two's complement, MSB top bit set) */

    AMS_Powertrain_Data_t expected = { .inverter_rpm = -1000 };
    b_Broker_Update_PowertrainData_ExpectAndReturn(&expected, true);

    parse_inverter_status(&pkt);
}

void test_parse_inverter_status_zero_rpm(void)
{
    CAN_RxPacket_t pkt = {0};
    pkt.std_id  = 0x181;
    pkt.data[0] = 0x00;
    pkt.data[1] = 0x00;

    AMS_Powertrain_Data_t expected = { .inverter_rpm = 0 };
    b_Broker_Update_PowertrainData_ExpectAndReturn(&expected, true);

    parse_inverter_status(&pkt);
}

/* =========================================================================
 * A Broker write failure (e.g. mutex timeout) must not crash — and since
 * the RPM is dropped, nothing further is expected (no retry, no fallback).
 * ========================================================================= */
void test_parse_inverter_status_broker_write_failure_does_not_crash(void)
{
    CAN_RxPacket_t pkt = {0};
    pkt.std_id  = 0x181;
    pkt.data[0] = 0xE8;
    pkt.data[1] = 0x03;

    AMS_Powertrain_Data_t expected = { .inverter_rpm = 1000 };
    b_Broker_Update_PowertrainData_ExpectAndReturn(&expected, false);

    parse_inverter_status(&pkt);
}
