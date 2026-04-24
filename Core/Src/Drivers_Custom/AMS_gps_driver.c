/**
 * @file    AMS_gps_driver.c
 * @brief   Custom GPS driver — HAL wrapper for USART6 idle-line DMA reception.
 *
 *  This driver is intentionally minimal. It owns:
 *    - The HAL UART handle pointer (bound at init time)
 *    - The private DMA receive buffer (MINMEA_MAX_SENTENCE_LENGTH + 2 bytes)
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
    /* One-shot idle-line DMA reception.
     * HAL automatically enables the DMA stream and the UART idle line interrupt.
     * When the RX line goes idle or the buffer fills, HAL calls
     * HAL_UARTEx_RxEventCallback() — overridden in AMS_GPS_Task.c. */
    HAL_UARTEx_ReceiveToIdle_DMA(s_phuart, s_dma_buf, sizeof(s_dma_buf));

    /* Disable the DMA half-transfer interrupt — we only want the idle/full event.
     * Without this, the callback fires at half-buffer too, sending incomplete sentences. */
    __HAL_DMA_DISABLE_IT(s_phuart->hdmarx, DMA_IT_HT);
}

const uint8_t *p_AMS_GPS_GetRxBuffer(void)
{
    return s_dma_buf;
}
