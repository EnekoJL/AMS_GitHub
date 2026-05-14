/**
 * @file    AMS_Logger_Task.h
 * @brief   Middleware RTOS Task: SD-card data logging + terminal broker printer.
 *
 *  Two independent sub-features share this task:
 *    1. SD-card CSV logger  (controlled by TASK_SD_CARD_ENABLE in AMS_task_config.h)
 *    2. Terminal broker printer (controlled by FEATURE_LOGGER_PRINT_ENABLE)
 *
 *  Both can be active simultaneously or independently.
 */

#ifndef MIDDLEWARE_AMS_LOGGER_TASK_H_
#define MIDDLEWARE_AMS_LOGGER_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialises the SD-card logger subsystem.
 *
 *        Mounts the filesystem, finds the next free filename (e.g. gekko_003.csv),
 *        opens the file, and writes the CSV header.
 *        Call this BEFORE starting the RTOS scheduler.
 *        Safe to call even when TASK_SD_CARD_ENABLE == 0 (returns immediately).
 */
void vd_Logger_Init(void);

/**
 * @brief Infinite RTOS task loop for the Logger.
 *
 *        On every iteration (period governed by osDelay inside the loop):
 *          - If TASK_SD_CARD_ENABLE == 1  → writes a CSV row to the SD card.
 *          - If FEATURE_LOGGER_PRINT_ENABLE == 1  → calls vd_Logger_PrintBrokerData()
 *            to dump all DataBroker structs to the terminal at LOGGER_PRINT_PERIOD_MS.
 *
 *        Invoke from 'SD_Card_Start' (or the equivalent task entry) in main.c / freertos.c.
 */
void vd_Logger_TaskProcess(void);

/**
 * @brief Dumps a formatted, ANSI-coloured snapshot of ALL DataBroker structs
 *        to the debug terminal via printf.
 *
 *        Each broker section (ADC, Vehicle, GPS, Telemetry, Persistent) is
 *        printed in its own colour-coded block, clearly separated by banners.
 *        The output is self-contained and human-readable at a glance.
 *
 *        Controlled at compile time by FEATURE_LOGGER_PRINT_ENABLE.
 *        If the flag is 0 this function compiles to nothing (zero overhead).
 */
void vd_Logger_PrintBrokerData(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_AMS_LOGGER_TASK_H_ */
