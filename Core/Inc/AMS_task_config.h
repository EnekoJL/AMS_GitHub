/**
 * @file    AMS_task_config.h
 * @brief   Compile-time configuration flags for RTOS tasks and sub-features.
 *
 *  Set a flag to 1 to ENABLE the feature, or 0 to DISABLE it.
 *
 *  TASK ENABLE FLAGS:
 *    - TASK_ADC_ENABLE          : ADC acquisition task
 *    - TASK_SD_CARD_ENABLE      : SD Card / Logger task
 *    - TASK_CAN_ENABLE          : CAN bus task
 *    - TASK_FLASH_MEMO_ENABLE   : Flash persistence task (thread creation)
 *
 *  SUB-FEATURE FLAGS:
 *    - FEATURE_FLASH_WRITE_ENABLE : When 0, b_Persist_SaveConfig() returns
 *                                   immediately WITHOUT touching flash.
 *                                   Use during testing to protect flash lifetime.
 *
 * @author  Eneko Juanena
 * @date    25 de Marzo de 2026
 */

#ifndef AMS_TASK_CONFIG_H_
#define AMS_TASK_CONFIG_H_

/* =========================================================================
 * RTOS Task enables (1 = create and run the task, 0 = skip task creation)
 * ========================================================================= */

#define TASK_ADC_ENABLE          0  /**< ADC acquisition task              */
#define TASK_SD_CARD_ENABLE      0  /**< SD Card / Logger task             */
#define TASK_CAN_ENABLE          0  /**< CAN bus task                      */
#define TASK_FLASH_MEMO_ENABLE   0  /**< Flash persistence task            */
#define TASK_GPS_ENABLE          1  /**< GPS NMEA task (USART6, DMA)       */

/* =========================================================================
 * Sub-feature enables
 * ========================================================================= */

/**
 * @brief Flash write protection for testing.
 *
 * Set to 0 during bench testing to prevent unnecessary flash write cycles.
 * The rest of the persistence logic (init, DataBroker) still runs normally.
 *
 * WARNING: Setting to 0 means NO data will be committed to flash.
 *          SOC and cycle_count WILL NOT persist across power cycles.
 */
#define FEATURE_FLASH_WRITE_ENABLE  0

#endif /* AMS_TASK_CONFIG_H_ */
