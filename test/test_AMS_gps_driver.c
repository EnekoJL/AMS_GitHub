/**
 * @file    test_AMS_gps_driver.c
 * @brief   Unit tests for Drivers_Custom/AMS_gps_driver.c — specifically the
 *          DMA-vs-IT reception fallback added when USART3 (bench-test UART,
 *          no DMA stream wired) needed to work alongside USART6 (production,
 *          DMA-wired). See the file header of AMS_gps_driver.c for the full
 *          story; this test locks in the exact branch this session's fix
 *          depends on: HAL_UARTEx_ReceiveToIdle_DMA() when hdmarx != NULL,
 *          HAL_UARTEx_ReceiveToIdle_IT() when hdmarx == NULL — and never
 *          both for the same call.
 *
 *          Mocks main.h (CMock) for HAL_UARTEx_ReceiveToIdle_DMA/IT — see
 *          test/vendor_stubs/main.h for the UART_HandleTypeDef/
 *          DMA_HandleTypeDef stub shapes.
 *
 *          ORDER MATTERS: s_phuart is a file-scope static in
 *          AMS_gps_driver.c that persists for the whole test binary's run
 *          (Unity does not restart the process between tests), same as
 *          test_AMS_DataBroker.c / test_AMS_Flash_Task.c. The "before
 *          Init()" test MUST run first, before anything else calls
 *          vd_AMS_GPS_Init() and leaves s_phuart non-NULL for good.
 */

#include "unity.h"
#include "mock_main.h"
#include "Drivers_Custom/AMS_gps_driver.h"

TEST_SOURCE_FILE("Drivers_Custom/AMS_gps_driver.c")

void setUp(void) {}
void tearDown(void) {}

/* =========================================================================
 * T00 — StartReceive() before Init() (s_phuart still NULL) must do nothing
 * — no mock expected, fails if either reception function gets called.
 * MUST run first — see the file header note on s_phuart's lifetime.
 * ========================================================================= */

void test_T00_StartReceive_before_init_does_nothing(void)
{
    vd_AMS_GPS_StartReceive();
}

/* =========================================================================
 * hdmarx != NULL (USART6 / production) -> DMA path armed, IT path untouched.
 * (CMock fails the test if HAL_UARTEx_ReceiveToIdle_IT is called without
 * being _Expect'd, so "IT untouched" is enforced implicitly.)
 * ========================================================================= */

void test_StartReceive_with_dma_wired_uses_dma_reception(void)
{
    DMA_HandleTypeDef dma;
    UART_HandleTypeDef huart = {0};
    huart.hdmarx = &dma;

    vd_AMS_GPS_Init(&huart);

    HAL_UARTEx_ReceiveToIdle_DMA_ExpectAndReturn(&huart, NULL, 256, HAL_OK);
    HAL_UARTEx_ReceiveToIdle_DMA_IgnoreArg_pData();

    vd_AMS_GPS_StartReceive();
}

/* =========================================================================
 * hdmarx == NULL (USART3 / bench test, no DMA stream wired) -> IT path
 * armed instead, without crashing on the NULL hdmarx.
 * ========================================================================= */

void test_StartReceive_without_dma_falls_back_to_interrupt_reception(void)
{
    UART_HandleTypeDef huart = {0};
    huart.hdmarx = NULL;

    vd_AMS_GPS_Init(&huart);

    HAL_UARTEx_ReceiveToIdle_IT_ExpectAndReturn(&huart, NULL, 256, HAL_OK);
    HAL_UARTEx_ReceiveToIdle_IT_IgnoreArg_pData();

    vd_AMS_GPS_StartReceive();
}

/* =========================================================================
 * GetRxBuffer always returns a valid (non-NULL) pointer to the same
 * internal buffer, regardless of which reception mode is active.
 * ========================================================================= */

void test_GetRxBuffer_returns_non_null_pointer(void)
{
    UART_HandleTypeDef huart = {0};
    vd_AMS_GPS_Init(&huart);

    TEST_ASSERT_NOT_NULL(p_AMS_GPS_GetRxBuffer());
}
