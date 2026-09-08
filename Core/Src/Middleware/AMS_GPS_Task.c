/**
 * @file    AMS_GPS_Task.c
 * @brief   RTOS Task for GPS NMEA reception and parsing (115200 baud, idle-line).
 *
 *          Bound UART is passed into vd_GPS_Task_Init() from main.c — see
 *          the comment at that call site. Currently USART3 (bench test,
 *          NMEA fed over the ST-LINK USB/VCP from a PC); production on the
 *          bike is USART6. Both work unchanged here — see AMS_gps_driver.c
 *          for how the driver picks DMA vs interrupt reception per UART.
 *
 * Architecture (ISR → Queue → Task → Broker):
 * ─────────────────────────────────────────────
 *  1. Driver arms reception (DMA or IT, depending on the UART) during
 *     vd_GPS_Task_Init().
 *  2. STM32 hardware writes incoming GPS bytes into the driver's buffer.
 *  3. When the RX line goes idle (end of NMEA sentence), HAL fires
 *     HAL_UARTEx_RxEventCallback() in ISR context.
 *  4. The callback calls vd_GPS_RxQueue_PostFromISR() → copies the sentence
 *     into a GPS_NmeaPacket_t and posts it to the FreeRTOS queue (timeout=0).
 *  5. The callback re-arms reception for the next sentence.
 *  6. vd_GPS_Manager_TaskProcess() wakes, drains the queue, parses with minmea,
 *     and calls b_Broker_Update_GPSData().
 *
 * Sentences parsed:
 *   $xxRMC — fix validity, lat/lon, speed, course, UTC time
 *   $xxGGA — fix quality, satellites tracked
 */

#include "Middleware/AMS_GPS_Task.h"
#include "Middleware/AMS_DataBroker.h"
#include "Middleware/minmea.h"
#include "Drivers_Custom/AMS_gps_driver.h"
#include "Algorithms/AMS_gps_algorithms.h"
#include "AMS_DataStructs.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <math.h>     /* fabsf */

/* -----------------------------------------------------------------------
 * Private types
 * ----------------------------------------------------------------------- */

#define GPS_DMA_BUF_SIZE 256

/** One slot in the RX queue — holds a complete raw NMEA burst. */
typedef struct {
    char     sentence[GPS_DMA_BUF_SIZE]; /* raw bytes  */
    uint16_t ui16_length;                /* byte count */
} GPS_NmeaPacket_t;

/* -----------------------------------------------------------------------
 * Private state
 * ----------------------------------------------------------------------- */

/** FreeRTOS message queue: 4 sentences of GPS_NmeaPacket_t each. */
static osMessageQueueId_t s_gps_rx_queue = NULL;

/* -----------------------------------------------------------------------
 * Private helpers
 * ----------------------------------------------------------------------- */

/**
 * @brief Creates the internal FreeRTOS RX queue.
 *        Called once from vd_GPS_Task_Init() — not part of the public API.
 */
static void prv_gps_rx_queue_create(void)
{
    s_gps_rx_queue = osMessageQueueNew(4u, sizeof(GPS_NmeaPacket_t), NULL);
}

/* -----------------------------------------------------------------------
 * ISR callback override (HAL weak function)
 * ----------------------------------------------------------------------- */

/**
 * @brief  HAL callback: fires when USART6 idle line is detected (end of NMEA sentence)
 *         or when the DMA buffer is completely full.
 *         This overrides the weak definition in the HAL library.
 *
 * @note   Called from DMA2_Stream1_IRQHandler / USART6_IRQHandler — ISR context.
 *         Must be ISR-safe: no blocking calls, no osDelay, no osMutexAcquire.
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *hUART, uint16_t ui16_size)
{
    if (hUART->Instance != USART6 && hUART->Instance != USART3) {
        return;
    }

    /* 1. Forward the raw sentence to the processing queue */
    vd_GPS_RxQueue_PostFromISR(p_AMS_GPS_GetRxBuffer(), ui16_size);

    /* 2. Re-arm DMA for the next sentence immediately */
    vd_AMS_GPS_StartReceive();
}

/**
 * @brief  HAL error callback: fires when Overrun (ORE), Noise, or Framing error occurs.
 *         Without this, an ORE will halt the UART DMA reception permanently.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3 || huart->Instance == USART6) {
        /* Clear errors and re-arm DMA so we don't get stuck */
        vd_AMS_GPS_StartReceive();
    }
}

/* -----------------------------------------------------------------------
 * Public implementation
 * ----------------------------------------------------------------------- */

void vd_GPS_Task_Init(UART_HandleTypeDef *phuart)
{
    /* 1. Create RX queue before enabling the interrupt */
    prv_gps_rx_queue_create();

    /* 2. Bind driver to the HAL handle */
    vd_AMS_GPS_Init(phuart);

    /* Enable USART interrupt for idle line detection if not already enabled */
    if (phuart->Instance == USART3) {
        HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    } else if (phuart->Instance == USART6) {
        HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
    }

    /* 3. Arm the first reception (DMA or IT — see AMS_gps_driver.c) */
    vd_AMS_GPS_StartReceive();

    printf("[GPS] Task initialized on %s @ 115200 baud (%s)\r\n",
           (phuart->Instance == USART3) ? "USART3" :
           (phuart->Instance == USART6) ? "USART6" : "USART?",
           (phuart->hdmarx != NULL) ? "DMA" : "IT");
}

void vd_GPS_RxQueue_PostFromISR(const uint8_t *p_data, uint16_t ui16_size)
{
    if (s_gps_rx_queue == NULL || p_data == NULL || ui16_size == 0u) {
        return;
    }

    GPS_NmeaPacket_t packet;
    uint16_t copy_len = (ui16_size < sizeof(packet.sentence))
                        ? ui16_size
                        : (uint16_t)(sizeof(packet.sentence) - 1u);

    memcpy(packet.sentence, p_data, copy_len);
    packet.sentence[copy_len] = '\0';
    packet.ui16_length = copy_len;

    /* Timeout = 0: never block in ISR context. Drop the packet if queue is full. */
    osMessageQueuePut(s_gps_rx_queue, &packet, 0u, 0u);
}

void vd_GPS_Manager_TaskProcess(void)
{
    GPS_NmeaPacket_t packet;
    GPS_Data_t       gps_snapshot = {0};

    /* Read the current Broker state so partial-sentence updates are additive.
     * If this fails (mutex timeout), gps_snapshot stays zeroed rather than
     * carrying stack garbage into the first Broker write below. */
    if (!b_Broker_Get_GPSData(&gps_snapshot)) {
        printf("[GPS] WARNING: initial Broker read failed, starting from zeroed state\r\n");
    }

    for (;;) {
        /* Block until a sentence arrives or timeout after 500 ms */
        osStatus_t status = osMessageQueueGet(s_gps_rx_queue, &packet, NULL, 500u);

        if (status != osOK) {
            /* No sentence received in 500 ms — GPS may be absent or no fix.
             * Mark stale but do not clear position data. */
            continue;
        }

        /* ---- Parse sentence(s) ---- */
        /* A single DMA burst may contain multiple back-to-back sentences.
           We loop through the buffer and extract them one by one. */
        char *line = packet.sentence;
        char *next_line;

        while (line != NULL && *line != '\0') {
            /* Find start of sentence */
            line = strchr(line, '$');
            if (line == NULL) {
                break; /* No more sentences in this burst */
            }

            /* Find end of sentence */
            next_line = strchr(line, '\n');
            if (next_line != NULL) {
                *next_line = '\0'; /* Null terminate this sentence */
                next_line++;       /* Move pointer to the next sentence */
            }

            /* Parse the single isolated sentence */
            switch (minmea_sentence_id(line, false)) {

                case MINMEA_SENTENCE_RMC: {
                    struct minmea_sentence_rmc frame;
                    if (minmea_parse_rmc(&frame, line)) {
                        gps_snapshot = st_GPS_ApplyRmcFrame(gps_snapshot, &frame, HAL_GetTick());
                        b_Broker_Update_GPSData(&gps_snapshot);
                    }
                } break;

                case MINMEA_SENTENCE_GGA: {
                    struct minmea_sentence_gga frame;
                    if (minmea_parse_gga(&frame, line)) {
                        gps_snapshot = st_GPS_ApplyGgaFrame(gps_snapshot, &frame);
                        b_Broker_Update_GPSData(&gps_snapshot);
                    }
                } break;

                default:
                    /* Unneeded sentence type */
                    break;
            }

            line = next_line;
        }
    }
}
