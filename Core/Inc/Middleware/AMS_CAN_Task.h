/**
 * @file    can_manager_task.h
 * @brief   RTOS Task API for managing CAN TX polling and RX via Queue.
 *
 * Architecture:
 *   ISR (HAL_CAN_RxFifo0MsgPendingCallback)
 *     └─> vd_CAN_RxQueue_PostFromISR()   [called from main.c callback]
 *           └─> osMessageQueue (FreeRTOS)
 *                 └─> vd_CAN_Manager_TaskProcess()  [RTOS thread - parses & updates Broker]
 */

#ifndef MIDDLEWARE_AMS_CAN_TASK_H_
#define MIDDLEWARE_AMS_CAN_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Inicializa la Queue interna de paquetes CAN RX.
 *        Debe llamarse antes de osKernelStart() en main.c.
 */
void vd_CAN_RxQueue_Init(void);

/**
 * @brief Publica un paquete CAN RAW en la Queue desde la ISR del callback.
 *        Es segura para llamarse desde contexto de interrupción.
 * @param rx_header   Puntero al header del mensaje recibido.
 * @param rx_data     Array de 8 bytes de datos del mensaje.
 */
void vd_CAN_RxQueue_PostFromISR(CAN_RxHeaderTypeDef *rx_header, uint8_t rx_data[8]);

/**
 * @brief Hilo principal del CAN para FreeRTOS.
 *        - Espera en la Queue los paquetes RX del Inversor.
 *        - Parsea y manda al DataBroker.
 *        - Gestiona el TX del botón azul.
 *        Debe invocarse desde 'CAN_Start' en main.c.
 */
void vd_CAN_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_CAN_TASK_H_ */
