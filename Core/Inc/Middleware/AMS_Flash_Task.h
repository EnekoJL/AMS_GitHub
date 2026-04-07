/**
 * @file    AMS_Flash_Task.h
 * @brief   Flash persistence middleware task.
 *          Circular-buffer read/write of records in internal Flash.
 *          Communicates with the DataBroker to load and save persistent config.
 *
 * Usage (from main.c task entry):
 *   void Flash_Memory_Start(void *argument) {
 *       vd_Persist_Task_Init();
 *       vd_Persist_TaskProcess();
 *   }
 *
 * @author  Eneko Juanena
 * @date    11 de Marzo de 2026
 */
#ifndef AMS_FLASH_TASK_H_
#define AMS_FLASH_TASK_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the Flash persistence subsystem for this task.
 *        Scans Flash sectors, loads the latest valid record into the DataBroker
 *        (or loads defaults if Flash is blank), and prints a boot diagnostic.
 *        Must be called once at the start of Flash_Memory_Start() before the task loop.
 */
void vd_Persist_Task_Init(void);

/**
 * @brief Infinite RTOS loop for the Flash persistence task.
 *        Placeholder for future scheduled save logic (e.g. periodic SOC save).
 *        Must be called from Flash_Memory_Start() after vd_Persist_Task_Init().
 */
void vd_Persist_TaskProcess(void);

/**
 * @brief Saves the current DataBroker config to Flash.
 *        Writes to the next free slot (circular). Erases the alternate sector
 *        and switches to it when the active sector is full.
 *
 * @retval true  if the write was successful.
 * @retval false if an error occurred or the module is not initialized.
 */
bool b_Persist_SaveConfig(void);

/**
 * @brief Returns the global slot index of the last written Flash record.
 *        Useful for debug output.
 * @retval Index of the last written slot (0 = none yet).
 */
uint32_t u32_Persist_GetLastSlotIndex(void);

#endif /* AMS_FLASH_TASK_H_ */
