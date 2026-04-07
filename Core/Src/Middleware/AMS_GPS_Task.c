/**
 * @file    AMS_GPS_Task.c
 * @brief   RTOS Task for GPS NMEA reception and parsing (USART6, 115200 baud, DMA).
 *
 * Architecture (ISR → Queue → Task → Broker):
 * ─────────────────────────────────────────────
 *  1. Driver arms HAL_UARTEx_ReceiveToIdle_DMA() during vd_GPS_Task_Init().
 *  2. STM32 hardware writes incoming GPS bytes into the driver's DMA buffer.
 *  3. When the RX line goes idle (end of NMEA sentence), HAL fires
 *     HAL_UARTEx_RxEventCallback() in ISR context.
 *  4. The callback calls vd_GPS_RxQueue_PostFromISR() → copies the sentence
 *     into a GPS_NmeaPacket_t and posts it to the FreeRTOS queue (timeout=0).
 *  5. The callback re-arms the DMA for the next sentence.
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
#include "AMS_DataStructs.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <math.h>     /* fabsf */

/* -----------------------------------------------------------------------
 * Private types
 * ----------------------------------------------------------------------- */

/** One slot in the RX queue — holds a complete raw NMEA sentence. */
typedef struct {
    char     sentence[MINMEA_MAX_SENTENCE_LENGTH + 2u]; /* raw bytes  */
    uint16_t ui16_length;                               /* byte count */
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
    if (hUART->Instance != USART6) {
        return;
    }

    /* 1. Forward the raw sentence to the processing queue */
    vd_GPS_RxQueue_PostFromISR(p_AMS_GPS_GetRxBuffer(), ui16_size);

    /* 2. Re-arm DMA for the next sentence immediately */
    vd_AMS_GPS_StartReceive();
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

    /* 3. Arm the first DMA reception */
    vd_AMS_GPS_StartReceive();

    printf("[GPS] Task initialized on USART6 @ 115200 baud\r\n");
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
    GPS_Data_t       gps_snapshot;

    /* Read the current Broker state so partial-sentence updates are additive */
    b_Broker_Get_GPSData(&gps_snapshot);

    for (;;) {
        /* Block until a sentence arrives or timeout after 500 ms */
        osStatus_t status = osMessageQueueGet(s_gps_rx_queue, &packet, NULL, 500u);

        if (status != osOK) {
            /* No sentence received in 500 ms — GPS may be absent or no fix.
             * Mark stale but do not clear position data. */
            continue;
        }

        /* ---- Parse sentence ---- */
        switch (minmea_sentence_id(packet.sentence, false)) {

            case MINMEA_SENTENCE_RMC: {
                struct minmea_sentence_rmc frame;
                if (minmea_parse_rmc(&frame, packet.sentence)) {
                    gps_snapshot.b_fix_valid = frame.valid;

                    if (frame.valid) {
                        /* Convert minmea fixed-point to micro-degrees (×1,000,000) */
                        float lat_deg = minmea_tocoord(&frame.latitude);
                        float lon_deg = minmea_tocoord(&frame.longitude);
                        gps_snapshot.i32_latitude_udeg  = (int32_t)(lat_deg * 1000000.0f);
                        gps_snapshot.i32_longitude_udeg = (int32_t)(lon_deg * 1000000.0f);

                        /* Speed: knots → km/h  (1 kn = 1.852 km/h) */
                        gps_snapshot.f_speed_kph  = minmea_tofloat(&frame.speed) * 1.852f;
                        gps_snapshot.f_course_deg = minmea_tofloat(&frame.course);

                        /* UTC time */
                        gps_snapshot.ui8_hour   = (uint8_t)frame.time.hours;
                        gps_snapshot.ui8_minute = (uint8_t)frame.time.minutes;
                        gps_snapshot.ui8_second = (uint8_t)frame.time.seconds;

                        gps_snapshot.ui32_last_fix_tick_ms = HAL_GetTick();
                    }

                    b_Broker_Update_GPSData(&gps_snapshot);
                }
            } break;

            case MINMEA_SENTENCE_GGA: {
                struct minmea_sentence_gga frame;
                if (minmea_parse_gga(&frame, packet.sentence)) {
                    gps_snapshot.ui8_fix_quality = (uint8_t)frame.fix_quality;
                    gps_snapshot.ui8_satellites  = (uint8_t)frame.satellites_tracked;
                    b_Broker_Update_GPSData(&gps_snapshot);
                }
            } break;

            default:
                /* MINMEA_UNKNOWN, MINMEA_INVALID, or unneeded sentence types — ignore */
                break;
        }
    }
}
