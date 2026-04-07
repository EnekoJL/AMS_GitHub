/**
 * @file    AMS_gps_driver.h
 * @brief   Custom GPS driver for AMS project.
 *          Thin HAL wrapper for USART6 idle-line DMA reception.
 *          Owns the private DMA buffer; exposes only Init and StartReceive.
 *
 * Usage (called from AMS_GPS_Task):
 *   vd_AMS_GPS_Init(&huart6);
 *   vd_AMS_GPS_StartReceive();   // arms first DMA transfer
 *   // Re-arm after each sentence: call vd_AMS_GPS_StartReceive() again
 *   //   from HAL_UARTEx_RxEventCallback inside AMS_GPS_Task.c
 */

#ifndef DRIVERS_CUSTOM_AMS_GPS_DRIVER_H_
#define DRIVERS_CUSTOM_AMS_GPS_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Binds the driver to a HAL UART handle (USART6).
 *        Must be called before vd_AMS_GPS_StartReceive().
 * @param phuart  Pointer to the HAL UART handle configured for DMA RX.
 */
void vd_AMS_GPS_Init(UART_HandleTypeDef *phuart);

/**
 * @brief Arms (or re-arms) a single idle-line DMA reception into the
 *        driver's internal buffer.
 *        Calls HAL_UARTEx_ReceiveToIdle_DMA().
 *        Must be called once during init and again after every
 *        HAL_UARTEx_RxEventCallback to keep the DMA running continuously.
 */
void vd_AMS_GPS_StartReceive(void);

/**
 * @brief Returns a pointer to the driver's internal DMA receive buffer.
 *        Valid only inside HAL_UARTEx_RxEventCallback (before re-arming).
 *        The caller must copy the data immediately — the buffer is reused
 *        on the next vd_AMS_GPS_StartReceive() call.
 * @retval Pointer to the raw DMA buffer (MINMEA_MAX_SENTENCE_LENGTH + 2 bytes).
 */
const uint8_t *p_AMS_GPS_GetRxBuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_CUSTOM_AMS_GPS_DRIVER_H_ */
