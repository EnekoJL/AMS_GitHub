/**
 * @file    AMS_can_driver.h
 * @brief   Custom CAN driver for AMS project.
 *          Provides a clean abstraction over STM32 HAL CAN.
 */

#ifndef DRIVERS_CUSTOM_AMS_CAN_DRIVER_H_
#define DRIVERS_CUSTOM_AMS_CAN_DRIVER_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Structure representing a standard CAN message.
 */
typedef struct {
    uint32_t std_id;  /**< Message standard ID */
    uint8_t  dlc;     /**< Data length code */
    uint8_t  data[8]; /**< Data payload */
} AMS_CAN_Msg_t;

/**
 * @brief Initializes the driver with a specific HAL CAN handle.
 * @param hcan Ptr to the HAL CAN handle.
 */
void vd_AMS_CAN_Init(CAN_HandleTypeDef *hcan);

/**
 * @brief Starts the CAN peripheral.
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef b_AMS_CAN_Start(void);

/**
 * @brief Transmits a standard CAN message.
 * @param std_id Standard ID of the message.
 * @param data   Pointer to the 8-byte data array.
 * @param dlc    Data length [0..8].
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef b_AMS_CAN_Transmit(uint32_t std_id, uint8_t *data, uint8_t dlc);

/**
 * @brief Returns the number of free TX mailboxes.
 * @retval uint32_t
 */
uint32_t u32_AMS_CAN_GetTxFreeLevel(void);

/**
 * @brief Configures a standard ID mask filter for CAN RX.
 * @param id   The ID to match.
 * @param mask The mask (1 means bit must match).
 * @param bank The filter bank to use.
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef b_AMS_CAN_ConfigureFilter(uint32_t id, uint32_t mask, uint32_t bank);

#endif /* DRIVERS_CUSTOM_AMS_CAN_DRIVER_H_ */
