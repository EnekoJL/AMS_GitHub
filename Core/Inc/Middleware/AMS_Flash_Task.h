/**
 * @file    AMS_Flash_Task.h
 * @brief   Flash persistence middleware task.
 *          Circular-buffer read/write of records in internal Flash.
 *          Communicates with the DataBroker to load and save persistent config.
 *
 * Usage:
 *   vd_Flash_Task_Init() must run from main(), before osKernelStart() —
 *   not from the task body — so the Broker holds real persisted config
 *   before any other task can read it.
 *     main() {
 *         ...
 *         vd_Flash_Task_Init();
 *         osKernelStart();
 *     }
 *     void Flash_Memory_Start(void *argument) {
 *         vd_Flash_Manager_TaskProcess();
 *     }
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
 *        Must be called once from main(), before osKernelStart() — not from
 *        the task body — so the Broker holds real persisted config before
 *        any other task starts reading it.
 */
void vd_Flash_Task_Init(void);

/**
 * @brief Infinite RTOS loop for the Flash persistence task.
 *        Placeholder for future scheduled save logic (e.g. periodic SOC save).
 *        Assumes vd_Flash_Task_Init() already ran in main().
 */
void vd_Flash_Manager_TaskProcess(void);

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
