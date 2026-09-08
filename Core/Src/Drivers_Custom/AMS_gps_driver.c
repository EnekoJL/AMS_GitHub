/**
 * @file    AMS_gps_driver.c
 * @brief   Custom GPS driver — HAL wrapper for idle-line UART reception.
 *
 *  This driver is intentionally minimal. It owns:
 *    - The HAL UART handle pointer (bound at init time)
 *    - The private receive buffer (MINMEA_MAX_SENTENCE_LENGTH + 2 bytes)
 *
 *  Two UARTs, two reception modes — both feed the same idle-line callback:
 *    - USART6 (production, on the bike): DMA is wired for this UART in
 *      stm32f4xx_hal_msp.c, so vd_AMS_GPS_StartReceive() arms
 *      HAL_UARTEx_ReceiveToIdle_DMA(). Zero CPU load while idle.
 *    - USART3 (bench testing over the ST-LINK USB/VCP, PC as the NMEA
 *      source): this UART has no DMA stream configured — it's the shared
 *      debug console (see README.md), and CubeMX only wires DMA for
 *      USART6. HAL_UARTEx_ReceiveToIdle_DMA would dereference a NULL
 *      hdmarx here, so this driver falls back to
 *      HAL_UARTEx_ReceiveToIdle_IT() automatically whenever hdmarx is NULL.
 *      Same HAL_UARTEx_RxEventCallback() fires either way — the rest of
 *      the GPS pipeline (AMS_GPS_Task.c) doesn't know or care which mode
 *      is active.
 *
 *  main() decides which UART to bind via vd_GPS_Task_Init()'s argument —
 *  see the comment at that call site for which one is currently wired.
 *
 *  All application logic (queue, parsing, Broker update) lives in AMS_GPS_Task.c.
 */

#include "Drivers_Custom/AMS_gps_driver.h"
#include "Middleware/minmea.h"  /* MINMEA_MAX_SENTENCE_LENGTH */
#include <string.h>

/* -----------------------------------------------------------------------
 * Private state
 * ----------------------------------------------------------------------- */

/** Bound HAL UART handle (set by vd_AMS_GPS_Init). */
static UART_HandleTypeDef *s_phuart = NULL;

#define GPS_DMA_BUF_SIZE 256

/**
 * DMA receive buffer. HAL_UARTEx_ReceiveToIdle_DMA writes GPS bytes here
 * directly. Increased to 256 to fit a full burst of back-to-back sentences.
 */
static uint8_t s_dma_buf[GPS_DMA_BUF_SIZE];

/* -----------------------------------------------------------------------
 * Public implementation
 * ----------------------------------------------------------------------- */

void vd_AMS_GPS_Init(UART_HandleTypeDef *phuart)
{
    s_phuart = phuart;
    memset(s_dma_buf, 0, sizeof(s_dma_buf));
}

void vd_AMS_GPS_StartReceive(void)
{
    if (s_phuart == NULL) {
        return;
    }

    if (s_phuart->hdmarx != NULL) {
        /* DMA path (USART6 / production): one-shot idle-line DMA reception.
         * HAL automatically enables the DMA stream and the UART idle line
         * interrupt. When the RX line goes idle or the buffer fills, HAL
         * calls HAL_UARTEx_RxEventCallback() — overridden in AMS_GPS_Task.c. */
        HAL_UARTEx_ReceiveToIdle_DMA(s_phuart, s_dma_buf, sizeof(s_dma_buf));

        /* Disable the DMA half-transfer interrupt — we only want the idle/full
         * event. Without this, the callback fires at half-buffer too, sending
         * incomplete sentences. */
        __HAL_DMA_DISABLE_IT(s_phuart->hdmarx, DMA_IT_HT);
    } else {
        /* Interrupt path (USART3 / bench test over ST-LINK VCP): this UART
         * has no DMA stream wired (see the file header comment), so use
         * byte-by-byte interrupt reception instead. HAL still detects the
         * idle line and calls the same HAL_UARTEx_RxEventCallback(). */
        HAL_UARTEx_ReceiveToIdle_IT(s_phuart, s_dma_buf, sizeof(s_dma_buf));
    }
}

const uint8_t *p_AMS_GPS_GetRxBuffer(void)
{
    return s_dma_buf;
}
