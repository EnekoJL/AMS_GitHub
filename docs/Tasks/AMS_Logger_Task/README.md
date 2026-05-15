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
   * Fetches data from the Data Broker (like 12V battery, suspension travel, and RPM) and appends a comma-separated row to the file.
   * Triggers a Green LED blink upon successful writes.
2. **Live Terminal Dashboard (`FEATURE_LOGGER_PRINT_ENABLE`):**
   * Acts as a live debugger, reading *every* struct available in the Data Broker (ADC, Vehicle, GPS, Telemetry, Persistence).
   * Uses VT100 ANSI escape codes (defined in `AMS_ansi_colors.h`) to draw styled boxes, distinct colored banners, and perfectly aligned key-value pairs.
   * Formats raw micro-degrees and millivolts into human-readable degrees and volts without using `printf` floating-point math (which consumes excessive stack space).
3. **Compile-Time Switches:**
   * Both sub-features are isolated by preprocessor macros defined in `AMS_task_config.h`. If either is disabled, the code compiles out entirely, saving flash and RAM.
