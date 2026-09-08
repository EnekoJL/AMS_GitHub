/**
 * @file    test_AMS_Led_Task.c
 * @brief   Unit tests for the LED state machine (Middleware/AMS_Led_Task.c).
 *
 *          Mocks AMS_led_driver.h (CMock) and HAL_GetTick (via the
 *          test/vendor_stubs/main.h stub), so this runs on a host with no
 *          real GPIO/HAL. Mocking HAL_GetTick means WE control exactly what
 *          "now" is, which is what lets TOGGLE/BLINK timing be tested
 *          deterministically instead of racing a real clock.
 *
 *          STATE NOTE: s_channels is a file-scope static array in
 *          AMS_Led_Task.c that persists for the whole test binary's run
 *          (Unity does not restart the process between tests). Unlike
 *          AMS_DataBroker's Init (which only runs once, guarded by a NULL
 *          check), vd_LED_Manager_Init() unconditionally resets every
 *          channel to OFF on every call — so every test below calls it
 *          first, and test order does NOT matter here (unlike
 *          test_AMS_DataBroker.c / test_AMS_Flash_Task.c).
 */

#include "unity.h"
#include "mock_AMS_led_driver.h"
#include "mock_main.h"
#include "Middleware/AMS_Led_Task.h"

TEST_SOURCE_FILE("Middleware/AMS_Led_Task.c")

/** Every test starts from a known state: all 4 LEDs OFF. */
static void prv_init(void)
{
    vd_LED_Driver_Init_Expect();
    vd_LED_Manager_Init();
}

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * SetMode(OFF) / SetMode(ON) — must call the matching driver Set function
 * for the correct color, immediately (not deferred to Process()).
 * ========================================================================= */

void test_SetMode_on_calls_driver_set_true_for_that_color(void)
{
    prv_init();

    vd_LED_Driver_SetRed_Expect(true);
    vd_LED_Manager_SetMode(LED_COLOR_RED, LED_PIN_ON);
}

void test_SetMode_off_calls_driver_set_false_for_that_color(void)
{
    prv_init();

    vd_LED_Driver_SetBlue_Expect(false);
    vd_LED_Manager_SetMode(LED_COLOR_BLUE, LED_PIN_OFF);
}

/* =========================================================================
 * SetMode(BLINK) must turn the LED on immediately and stamp a deadline
 * HAL_GetTick() + LED_BLINK_DURATION_*_ms into the future.
 * ========================================================================= */

void test_SetMode_blink_turns_on_immediately_and_stamps_deadline(void)
{
    prv_init();

    vd_LED_Driver_SetGreen_Expect(true);
    HAL_GetTick_ExpectAndReturn(1000);
    vd_LED_Manager_SetMode(LED_COLOR_GREEN, LED_PIN_BLINK);
    /* deadline = 1000 + 200 (LED_BLINK_DURATION_GREEN_ms) = 1200, verified
     * indirectly below by checking Process() behavior right at that tick. */

    /* Just before the deadline: still on, no driver call. */
    HAL_GetTick_ExpectAndReturn(1199);
    vd_LED_Manager_Process();

    /* At the deadline: turns off and the channel reverts to OFF. */
    HAL_GetTick_ExpectAndReturn(1200);
    vd_LED_Driver_SetGreen_Expect(false);
    vd_LED_Manager_Process();

    /* Now OFF — a further Process() call must not touch the driver again. */
    vd_LED_Manager_Process();
}

/* =========================================================================
 * SetMode(TOGGLE) must NOT touch the driver immediately — only seeds the
 * toggle timer. The actual toggling happens in Process().
 * ========================================================================= */

void test_SetMode_toggle_does_not_touch_driver_immediately(void)
{
    prv_init();

    HAL_GetTick_ExpectAndReturn(5000); /* seeds toggle_tick, no driver call */
    vd_LED_Manager_SetMode(LED_COLOR_ORANGE, LED_PIN_TOGGLE);
}

/* =========================================================================
 * Process() on a TOGGLE channel: before the 500ms period, no toggle;
 * at/after the period, toggles and re-seeds the timer.
 * ========================================================================= */

void test_Process_toggle_does_not_fire_before_period_elapses(void)
{
    prv_init();
    HAL_GetTick_ExpectAndReturn(0);
    vd_LED_Manager_SetMode(LED_COLOR_RED, LED_PIN_TOGGLE);

    HAL_GetTick_ExpectAndReturn(499); /* 499ms elapsed, period is 500ms */
    vd_LED_Manager_Process();         /* no toggle call expected */
}

void test_Process_toggle_fires_at_period_and_reseeds_timer(void)
{
    prv_init();
    HAL_GetTick_ExpectAndReturn(0);
    vd_LED_Manager_SetMode(LED_COLOR_RED, LED_PIN_TOGGLE);

    HAL_GetTick_ExpectAndReturn(500);  /* period check */
    HAL_GetTick_ExpectAndReturn(500);  /* re-seed toggle_tick */
    vd_LED_Driver_ToggleRed_Expect();
    vd_LED_Manager_Process();

    /* Immediately after: dt is 0 again, must not re-fire. */
    HAL_GetTick_ExpectAndReturn(500);
    vd_LED_Manager_Process();
}

/* =========================================================================
 * Independence: setting one channel must not affect another channel's
 * state. BLUE toggling must not cause GREEN (left ON) to receive any call.
 * ========================================================================= */

void test_channels_are_independent(void)
{
    prv_init();

    vd_LED_Driver_SetGreen_Expect(true);
    vd_LED_Manager_SetMode(LED_COLOR_GREEN, LED_PIN_ON);

    HAL_GetTick_ExpectAndReturn(0);
    vd_LED_Manager_SetMode(LED_COLOR_BLUE, LED_PIN_TOGGLE);

    /* Process(): BLUE's toggle period elapses, GREEN must receive nothing. */
    HAL_GetTick_ExpectAndReturn(600);
    HAL_GetTick_ExpectAndReturn(600);
    vd_LED_Driver_ToggleBlue_Expect();
    vd_LED_Manager_Process();
}

/* =========================================================================
 * Invalid color must be ignored, not crash and not touch the driver.
 * ========================================================================= */

void test_SetMode_invalid_color_is_ignored(void)
{
    prv_init();

    vd_LED_Manager_SetMode((AMS_LED_Color_t)99, LED_PIN_ON); /* no mock expected -> fails if driver touched */
}
