# AMS_Logger_Task

## Overview
The `AMS_Logger_Task` is a dual-purpose diagnostics and telemetry recording task. It handles appending real-time system data to an SD card for post-drive analysis, and it prints highly-formatted, ANSI-colored debug dashboards to the serial terminal.

## Execution Model: Periodic Polling
The task runs a continuous periodic loop (`osDelay(100)`) to check if logging or printing operations need to occur.

1. **SD Card Sub-Feature:** Executes every loop iteration (100ms default) to ensure fast data logging without blocking other tasks.
2. **Terminal Print Sub-Feature:** Gated by a separate timer (`LOGGER_PRINT_PERIOD_MS`), which limits serial output spam while allowing SD logging to happen much faster.

## Core Features
1. **SD-Card CSV Logging (`TASK_SD_CARD_ENABLE`):**
   * Automatically mounts the SD card using the FATFS driver upon initialization.
   * Searches for the next available filename (e.g., `gekko_01.csv`, `gekko_02.csv`) to prevent overwriting past logs.
   * Every `LOGGER_SD_PERIOD_MS` (default 100 ms, independent of the terminal print rate), fetches one full `AMS_Data_t` snapshot from the Data Broker — vehicle state, GPS, telemetry, BMS — and appends it as one CSV row.
   * Each row also carries a fresh/stale flag per domain (`VEH_FRESH`, `ADC_FRESH`, `GPS_FRESH`, `BMS_FRESH`, `TELEM_FRESH`). Slower-updating domains (e.g. BMS) repeat their last value across several rows between real updates — the flag tells you whether a given row's value is a new sample or a repeat, so post-processing doesn't misread a held-over value as a fresh reading.
   * `AMS_BMS_Data_t` columns will read `0` until a BMS driver/CAN parser task exists and starts calling `b_Broker_Update_BMSData()` — the struct is a placeholder for now (see `Architecture_Overview.md`).
   * Triggers a Green LED blink upon successful writes.
2. **Live Terminal Dashboard (`FEATURE_LOGGER_PRINT_ENABLE`):**
   * Acts as a live debugger, reading *every* struct available in the Data Broker (ADC, Vehicle, GPS, Telemetry, Persistence).
   * Uses VT100 ANSI escape codes (defined in `AMS_ansi_colors.h`) to draw styled boxes, distinct colored banners, and perfectly aligned key-value pairs.
   * Formats raw micro-degrees and millivolts into human-readable degrees and volts without using `printf` floating-point math (which consumes excessive stack space).
3. **Compile-Time Switches:**
   * Both sub-features are isolated by preprocessor macros defined in `AMS_task_config.h`. If either is disabled, the code compiles out entirely, saving flash and RAM.
