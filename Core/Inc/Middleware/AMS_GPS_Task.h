/**
 * @file    AMS_GPS_Task.h
 * @brief   RTOS Task API for GPS NMEA reception and parsing.
 *
 * Architecture:
 *   ISR (HAL_UARTEx_RxEventCallback — idle-line event on USART6)
 *     └─> vd_GPS_RxQueue_PostFromISR()   [ISR-safe, copies sentence into queue]
 *           └─> osMessageQueue (FreeRTOS, 4 slots)
 *                 └─> vd_GPS_Manager_TaskProcess()  [RTOS thread: minmea parse & Broker update]
 *
 * Usage (from main.c task entry):
 *   void GPS_Start_Task(void *argument) {
 *       vd_GPS_Task_Init(&huart6);
 *       vd_GPS_Manager_TaskProcess();
 *   }
 */

#ifndef MIDDLEWARE_AMS_GPS_TASK_H_
#define MIDDLEWARE_AMS_GPS_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/**
 * @brief Initializes the GPS task subsystem.
 *        Performs in order: RX queue creation, driver binding,
 *        and first idle-line DMA reception arm.
 *        Must be called once at the start of GPS_Start_Task() before the loop.
 *
 * @param phuart  Pointer to the USART6 HAL handle.
 */
void vd_GPS_Task_Init(UART_HandleTypeDef *phuart);

/**
 * @brief Posts a raw NMEA sentence into the RX queue from HAL_UARTEx_RxEventCallback.
 *        ISR-safe (uses osMessageQueuePut with timeout=0).
 * @param p_data  Pointer to the received bytes (driver's DMA buffer).
 * @param ui16_size  Number of bytes received (sentence length).
 */
void vd_GPS_RxQueue_PostFromISR(const uint8_t *p_data, uint16_t ui16_size);

/**
 * @brief Infinite RTOS loop: drains the sentence queue, parses NMEA with minmea,
 *        and updates GPS_Data_t in the DataBroker.
 *        Must be called from GPS_Start_Task() after vd_GPS_Task_Init().
 */
void vd_GPS_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_GPS_TASK_H_ */
