/**
 * @file    AMS_can_driver.c
 * @brief   Implementation of the custom CAN driver wrappers.
 */

#include "Drivers_Custom/AMS_can_driver.h"
#include <stddef.h>

static CAN_HandleTypeDef *s_hcan = NULL;

void vd_AMS_CAN_Init(CAN_HandleTypeDef *phcan) {
    s_hcan = phcan;
}

HAL_StatusTypeDef b_AMS_CAN_Start(void) {
    if (s_hcan == NULL) return HAL_ERROR;
    
    HAL_StatusTypeDef status = HAL_CAN_Start(s_hcan);
    if (status == HAL_OK) {
        /* Enable RX FIFO0 interrupt */
        status = HAL_CAN_ActivateNotification(s_hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    }
    return status;
}

HAL_StatusTypeDef b_AMS_CAN_Transmit(uint32_t std_id, uint8_t *data, uint8_t dlc) {
    if (s_hcan == NULL || data == NULL) return HAL_ERROR;
    
    CAN_TxHeaderTypeDef tx_header;
    uint32_t mailbox;
    
    tx_header.StdId = std_id;
    tx_header.ExtId = 0;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = dlc;
    tx_header.TransmitGlobalTime = DISABLE;
    
    return HAL_CAN_AddTxMessage(s_hcan, &tx_header, data, &mailbox);
}

uint32_t u32_AMS_CAN_GetTxFreeLevel(void) {
    if (s_hcan == NULL) return 0;
    return HAL_CAN_GetTxMailboxesFreeLevel(s_hcan);
}

HAL_StatusTypeDef b_AMS_CAN_ConfigureFilter(uint32_t id, uint32_t mask, uint32_t bank) {
    if (s_hcan == NULL) return HAL_ERROR;
    
    CAN_FilterTypeDef canfilterconfig;

    canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
    canfilterconfig.FilterBank = bank;
    canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    canfilterconfig.FilterIdHigh = (uint16_t)(id << 5);
    canfilterconfig.FilterIdLow = 0x0000;
    canfilterconfig.FilterMaskIdHigh = (uint16_t)(mask << 5);
    canfilterconfig.FilterMaskIdLow = 0x0000;
    canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
    canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
    canfilterconfig.SlaveStartFilterBank = 14;

    return HAL_CAN_ConfigFilter(s_hcan, &canfilterconfig);
}
