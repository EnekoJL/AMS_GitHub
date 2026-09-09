/**
 * @file    AMS_BMS_Task.h
 * @brief   RTOS Task API for the LTC6813 battery-pack BMS.
 *
 * Execution model: periodic polling, same shape as AMS_Algorithms_Task.
 *   vd_BMS_Task_Init()          -> binds SPI handle, configures both ICs
 *   vd_BMS_Manager_TaskProcess() -> for(;;) { measure -> safety check ->
 *                                    write AMS_BMS_Data_t to Broker -> osDelay }
 *
 * Usage (from main.c task entry):
 *   void BMS_Start(void *argument) {
 *       vd_BMS_Task_Init(&hspi2);
 *       vd_BMS_Manager_TaskProcess();
 *   }
 */
#ifndef MIDDLEWARE_AMS_BMS_TASK_H_
#define MIDDLEWARE_AMS_BMS_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief Binds the driver to a HAL SPI handle and configures both LTC6813
 *        ICs. Must be called once before vd_BMS_Manager_TaskProcess().
 * @param phspi  Pointer to the HAL SPI handle (SPI2).
 */
void vd_BMS_Task_Init(SPI_HandleTypeDef *phspi);

/**
 * @brief Infinite RTOS loop: measures the pack, runs the per-cell
 *        voltage safety check, and writes the result to the Broker.
 *        Must be called from BMS_Start() after vd_BMS_Task_Init().
 */
void vd_BMS_Manager_TaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_BMS_TASK_H_ */
