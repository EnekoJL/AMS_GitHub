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
 *    - TASK_GPS_ENABLE          : GPS NMEA task (USART3 bench-test wiring by
 *                                      default, DMA on USART6 in production
 *                                      — see the comment at vd_GPS_Task_Init()'s
 *                                      call site in main.c)
 *    - TASK_BMS_ENABLE          : LTC6813 BMS task (SPI2 — see
 *                                      Drivers_Custom/AMS_bms_driver.h for
 *                                      pin wiring)
 *
 *  SUB-FEATURE FLAGS:
 *    - FEATURE_FLASH_WRITE_ENABLE    : When 0, b_Persist_SaveConfig() returns
 *                                      immediately WITHOUT touching flash.
 *                                      Use during testing to protect flash lifetime.
 *    - FEATURE_LOGGER_PRINT_ENABLE   : When 1, the Logger task periodically dumps
 *                                      all DataBroker structs to the SWO/UART
 *                                      terminal with ANSI colour formatting.
 *                                      Independent of SD-card logging.
 *    - LOGGER_PRINT_PERIOD_MS        : Interval between terminal prints (default 1 Hz).
 *    - LOGGER_SD_PERIOD_MS           : Interval between SD-card CSV rows.
 *
 * @author  Eneko Juanena
 * @date    25 de Marzo de 2026
 */

#ifndef AMS_TASK_CONFIG_H_
#define AMS_TASK_CONFIG_H_

/* =========================================================================
 * RTOS Task enables (1 = create and run the task, 0 = skip task creation)
 * ========================================================================= */

#define TASK_ADC_ENABLE          1  /**< ADC acquisition task              */
#define TASK_SD_CARD_ENABLE      0  /**< SD Card / Logger task             */
#define TASK_CAN_ENABLE          0  /**< CAN bus task                      */
#define TASK_FLASH_MEMO_ENABLE   0  /**< Flash persistence task            */
#define TASK_GPS_ENABLE          0  /**< GPS NMEA task (USART6, DMA)       */
#define TASK_BMS_ENABLE          0  /**< LTC6813 BMS task (SPI2)           */

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

/**
 * @brief Terminal broker data printer.
 *
 * Set to 1 to enable periodic ANSI-coloured printf of ALL DataBroker structs
 * from within the Logger task.  This is completely independent of the SD-card
 * logger — both can be active simultaneously or individually.
 *
 * Set to 0 to silence the terminal output entirely (zero CPU overhead).
 */
#define FEATURE_LOGGER_PRINT_ENABLE  1

/**
 * @brief Print period in milliseconds.
 *
 * Default: 1000 ms  →  1 Hz refresh rate.
 * Increase for less terminal spam (e.g. 5000 for 0.2 Hz).
 * Decrease for higher resolution (min recommended: 200 ms).
 */
#define LOGGER_PRINT_PERIOD_MS  1000U

/**
 * @brief SD-card CSV row period in milliseconds.
 *
 * Every row is a full snapshot of AMS_Data_t (vehicle, gps, telemetry, bms,
 * safety flags) — decoupled from LOGGER_PRINT_PERIOD_MS on purpose, since
 * "how often should we persist a row to disk" and "how often should the
 * terminal dashboard refresh" are different concerns.
 *
 * Slow-changing columns (e.g. BMS) will repeat their last value across
 * several rows until their producer task updates them — check that
 * column's _FRESH flag in the same row before trusting it as a new sample.
 */
#define LOGGER_SD_PERIOD_MS  100U

#endif /* AMS_TASK_CONFIG_H_ */
