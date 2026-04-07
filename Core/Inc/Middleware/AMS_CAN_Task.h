/**
 * @file    AMS_CAN_Task.h
 * @brief   RTOS Task API for managing CAN TX polling and RX via Queue.
 *
 * Architecture:
 *   ISR (HAL_CAN_RxFifo0MsgPendingCallback in main.c)
 *     └─> vd_CAN_RxQueue_PostFromISR()   [ISR-safe, called from HAL callback]
 *           └─> osMessageQueue (FreeRTOS)
 *                 └─> vd_CAN_Manager_TaskProcess()  [RTOS thread: parse & update Broker]
 *
 * Usage (from main.c task entry):
 *   void CAN_Start(void *argument) {
 *       vd_CAN_Task_Init(&hcan2);
 *       vd_CAN_Manager_TaskProcess();
 *   }
 */

#ifndef MIDDLEWARE_AMS_CAN_TASK_H_
#define MIDDLEWARE_AMS_CAN_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Initializes the CAN task subsystem.
 *        Performs in order: RX queue creation, driver binding, filter
 *        configuration, and CAN peripheral start with RX interrupt enabled.
 *        Must be called once at the start of CAN_Start() before the task loop.
 *
 * @param phcan  Pointer to the CAN2 peripheral handle.
 */
void vd_CAN_Task_Init(CAN_HandleTypeDef *phcan);

/**
 * @brief Posts a raw CAN packet into the RX queue from the HAL ISR callback.
 *        ISR-safe (uses osMessageQueuePut with timeout=0).
 * @param rx_header  Pointer to the received message header.
 * @param rx_data    8-byte raw payload.
 */
void vd_CAN_RxQueue_PostFromISR(CAN_RxHeaderTypeDef *rx_header, uint8_t rx_data[8]);

/**
 * @brief Infinite RTOS loop: drains the RX queue, parses messages into the
 *        DataBroker, and polls the blue button for TX.
 *        Must be called from CAN_Start() after vd_CAN_Task_Init().
 */
void vd_CAN_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_CAN_TASK_H_ */
